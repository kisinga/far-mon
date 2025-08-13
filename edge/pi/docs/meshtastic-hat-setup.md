# Raspberry Pi with Meshtastic Hat Setup

This document covers how to modify the farm stack when using a Raspberry Pi with a Meshtastic-compatible LoRa hat, making the Pi itself a Meshtastic node rather than just a relay for external devices.

## 🔀 Architecture Comparison

### Standard Setup (Pi as Relay)
```
Internet ── Tailscale ── Pi (Farm Stack + WiFi Relay) ── Heltec Devices ── LoRa Mesh
```

### Meshtastic Hat Setup (Pi as Node)
```
Internet ── Tailscale ── Pi (Farm Stack + Meshtastic Node) ── LoRa Mesh
                                      ↑
                                 LoRa Hat (SX1262/SX1276)
```

## 📡 Compatible Meshtastic Hats

### Recommended Hats
| Hat | Chip | Frequency | Status | Notes |
|-----|------|-----------|--------|-------|
| **Waveshare SX1262 LoRa HAT** | SX1262 | 433/868/915 MHz | ✅ Supported | Best performance, long range |
| **Adafruit LoRa Radio Bonnet** | RFM95W (SX1276) | 433/868/915 MHz | ✅ Supported | Reliable, good community support |
| **Dragino LoRa/GPS HAT** | SX1276/SX1278 | 433/868/915 MHz | ✅ Supported | Includes GPS for position |
| **RAK2245 Pi HAT** | SX1301 | 868/915 MHz | ⚠️ Limited | LoRaWAN focused, needs adaptation |

### For Kenya (868 MHz ISM Band)
- **Waveshare SX1262 LoRa HAT (868MHz)** - Best choice
- **Adafruit LoRa Radio Bonnet RFM95W 868MHz** - Good alternative

## 🏗️ Modified Architecture

### Service Changes
```yaml
# Modified docker-compose.yml services
services:
  mosquitto:     # Same - MQTT broker
  nodered:       # Same - Automation & dashboard
  influxdb:      # Same - Time-series storage
  
  # NEW: Meshtastic service running on Pi
  meshtastic:
    image: meshtastic/python-cli:latest
    container_name: farm-meshtastic
    restart: unless-stopped
    privileged: true  # For GPIO/SPI access
    devices:
      - "/dev/spidev0.0:/dev/spidev0.0"  # SPI for LoRa hat
      - "/dev/gpiomem:/dev/gpiomem"      # GPIO access
    volumes:
      - meshtastic_config:/home/meshtastic/.config
      - /etc/localtime:/etc/localtime:ro
    environment:
      - TZ=Africa/Nairobi
      - MESHTASTIC_MQTT_HOST=mosquitto
      - MESHTASTIC_MQTT_PORT=1883
    networks:
      - farm_network
    command: |
      sh -c "
        # Configure Meshtastic for direct MQTT integration
        meshtastic --set mqtt.enabled true
        meshtastic --set mqtt.address mosquitto
        meshtastic --set mqtt.port 1883
        meshtastic --set mqtt.root farm/mesh
        # Start MQTT bridge service
        python3 -m meshtastic.mqtt_bridge
      "
```

## 🔧 Setup Script Modifications

### Modified `setup_farm_pi.sh`
```bash
# Additional function for Meshtastic hat setup
configure_meshtastic_hat() {
    log_info "Configuring Meshtastic hat support..."
    
    # Enable SPI interface for LoRa hat
    raspi-config nonint do_spi 0
    
    # Add required kernel modules
    echo "spi-bcm2835" >> /etc/modules
    echo "spidev" >> /etc/modules
    
    # Install Meshtastic Python CLI
    pip3 install --upgrade meshtastic
    
    # Install CircuitPython libraries for some hats
    pip3 install adafruit-circuitpython-rfm9x
    
    # Set up GPIO permissions
    usermod -a -G gpio,spi "$ACTUAL_USER"
    
    log_success "Meshtastic hat support configured"
}

# Add to main() function
main() {
    check_requirements
    update_system
    install_prerequisites
    configure_system
    configure_meshtastic_hat  # NEW
    install_docker
    install_tailscale
    setup_tailscale
    install_coolify
    setup_repository
    create_service_dirs
    verify_setup
    show_final_instructions
}
```

### Hardware Detection Script
```bash
# utils/detect_meshtastic_hat.sh
#!/bin/bash
# Detect and configure Meshtastic-compatible hats

detect_hat() {
    log_info "Detecting Meshtastic-compatible hats..."
    
    # Check for common hat EEPROMs
    if [ -f /proc/device-tree/hat/vendor ]; then
        VENDOR=$(cat /proc/device-tree/hat/vendor)
        PRODUCT=$(cat /proc/device-tree/hat/product)
        log_info "Detected HAT: $VENDOR $PRODUCT"
        
        case "$VENDOR" in
            "Waveshare")
                configure_waveshare_sx1262
                ;;
            "Adafruit")
                configure_adafruit_lora
                ;;
            "Dragino")
                configure_dragino_hat
                ;;
            *)
                log_warning "Unknown HAT vendor, using generic SPI configuration"
                configure_generic_spi
                ;;
        esac
    else
        log_info "No HAT EEPROM detected, checking manual configuration..."
        configure_generic_spi
    fi
}

configure_waveshare_sx1262() {
    log_info "Configuring Waveshare SX1262 LoRa HAT..."
    
    # Waveshare SX1262 pin configuration
    cat > /opt/farm/meshtastic-config.yaml << EOF
device:
  role: ROUTER  # or CLIENT for end nodes
  serial_enabled: true
  
lora:
  frequency_offset: 0
  tx_power: 14  # dBm, adjust for regulations
  bandwidth: 250
  spreading_factor: 9
  coding_rate: 8

mqtt:
  enabled: true
  address: "mosquitto"
  port: 1883
  username: ""
  password: ""
  encryption_enabled: false
  json_enabled: true
  root: "farm/mesh"

position:
  gps_enabled: false  # Enable if GPS module present
  position_broadcast_secs: 900  # 15 minutes

network:
  wifi_ssid: ""  # Pi handles connectivity
  wifi_password: ""
EOF
    
    log_success "Waveshare SX1262 configuration created"
}

configure_adafruit_lora() {
    log_info "Configuring Adafruit LoRa Radio Bonnet..."
    
    # Adafruit RFM95W pin configuration
    # Similar config but different GPIO pins
    log_success "Adafruit LoRa configuration created"
}
```

## 🔄 Modified Data Flow

### Direct Integration Flow
```
Field Sensors ── LoRa Mesh ── Pi (LoRa Hat) ── Meshtastic Service ── Local MQTT ── Node-RED ── InfluxDB
                                    ↑
                              Built-in Gateway
```

### Benefits
- **No WiFi relay needed** - Pi is directly on the mesh
- **Lower latency** - Direct MQTT integration
- **Simplified network** - No 10.42.0.x subnet needed
- **Better reliability** - Fewer wireless hops
- **Integrated position** - Pi can broadcast its GPS location

### Trade-offs
- **Hardware dependency** - Requires compatible hat
- **Limited range positioning** - Pi location is fixed
- **Single point of failure** - Pi down = gateway down
- **Cost** - Additional hat cost (~$30-60)

## 🔧 Configuration Changes

### No Meshtastic Relay Needed
When using a hat, **skip** the Meshtastic relay setup entirely:

```bash
# Standard setup (foundation + farm stack)
curl -fsSL https://raw.githubusercontent.com/your-org/farm/main/edge/pi/utils/setup_farm_pi.sh | sudo bash

# Deploy farm stack via Coolify (same as before)
# No need for: install_meshtastic_relay.sh
```

### Modified Docker Compose
Add Meshtastic service to `docker-compose.yml`:

```yaml
  meshtastic:
    image: python:3.11-slim
    container_name: farm-meshtastic
    restart: unless-stopped
    privileged: true
    devices:
      - "/dev/spidev0.0:/dev/spidev0.0"
      - "/dev/gpiomem:/dev/gpiomem"
    volumes:
      - meshtastic_config:/config
      - ./meshtastic/startup.sh:/startup.sh:ro
    environment:
      - TZ=Africa/Nairobi
      - PYTHONUNBUFFERED=1
    networks:
      - farm_network
    depends_on:
      - mosquitto
    command: ["/startup.sh"]

volumes:
  meshtastic_config:
    driver: local
    driver_opts:
      type: none
      o: bind
      device: /srv/meshtastic
```

### Startup Script
```bash
#!/bin/bash
# meshtastic/startup.sh
pip install meshtastic adafruit-circuitpython-rfm9x

# Configure Meshtastic
meshtastic --configure /config/meshtastic-config.yaml

# Start MQTT bridge
python3 -c "
import meshtastic.mqtt_interface
import time

# Connect to local device via serial
interface = meshtastic.mqtt_interface.MQTTInterface(
    host='mosquitto',
    port=1883,
    topic_prefix='farm/mesh'
)

# Keep running
while True:
    time.sleep(1)
"
```

## 📊 Node-RED Integration

### Direct Device Access
With a hat, Node-RED can directly communicate with the Meshtastic device:

```javascript
// Node-RED function node
const { exec } = require('child_process');

// Send message to mesh
exec('meshtastic --sendtext "Water level: 85%"', (error, stdout) => {
    if (error) {
        node.error(`Meshtastic error: ${error}`);
        return;
    }
    msg.payload = stdout;
    node.send(msg);
});
```

### MQTT Topics
```
farm/mesh/2/json/position     # Position updates
farm/mesh/2/json/nodeinfo     # Node information
farm/mesh/2/json/telemetry    # Device telemetry
farm/mesh/c/json/message      # Chat messages
```

## 🛠️ Troubleshooting Hat Setup

### Common Issues
| Issue | Solution |
|-------|----------|
| SPI not detected | Run `raspi-config` → Interface → SPI → Enable |
| Permission denied | Add user to `gpio` and `spi` groups |
| Hat not responding | Check physical connections and power |
| Wrong frequency | Verify hat frequency matches region (868MHz for Kenya) |

### Diagnostic Commands
```bash
# Check SPI interface
ls -la /dev/spi*

# Test Meshtastic connectivity
meshtastic --info

# Monitor MQTT messages
mosquitto_sub -h localhost -t "farm/mesh/+/json/+"

# Check GPIO access
gpio readall  # If wiringpi installed
```

## 🔄 Migration Path

### From Relay Setup to Hat Setup
1. **Deploy hat-compatible Pi** with farm stack
2. **Configure hat** using detection script
3. **Update Meshtastic nodes** to use new gateway
4. **Migrate data flows** in Node-RED
5. **Decommission relay Pi** (or repurpose)

The hat setup provides a more integrated, reliable solution at the cost of hardware flexibility and positioning options.
