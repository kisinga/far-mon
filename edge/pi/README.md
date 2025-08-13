# Raspberry Pi Farm Monitoring Stack

This directory contains the complete farm monitoring infrastructure for Raspberry Pi, including the core monitoring services and optional Meshtastic Heltec device access relay.

## 🚜 Complete Setup Flow

### 1. Foundation Setup (Tailscale + Coolify)

```bash
# One-line foundation setup
curl -fsSL https://raw.githubusercontent.com/your-org/farm/main/edge/pi/utils/setup_farm_pi.sh | sudo bash
```

This installs:
- ✅ Tailscale VPN for secure remote access
- ✅ Coolify for container orchestration  
- ✅ Docker and system hardening
- ✅ Service directories and prerequisites

### 2. Deploy Farm Services via Coolify

1. **Access Coolify Dashboard**: `http://YOUR_PI_IP:8000` or `http://TAILSCALE_IP:8000`
2. **Add this Pi as a server** in Coolify
3. **Deploy the stack**: Upload `docker-compose.yml` from this directory
4. **Configure services**: Update passwords and tokens in environment variables

**Core Farm Services Deployed:**
- 🦟 **Mosquitto MQTT** (port 1883) - Message broker for sensors
- 🔴 **Node-RED** (port 1880) - Automation and dashboard  
- 📊 **InfluxDB** (port 8086) - Time-series database

**Optional Services (uncomment in docker-compose.yml):**
- 🏢 **ThingsBoard** (port 8080) - IoT platform with PostgreSQL backend

### 3. Add Meshtastic Relay (Optional)

```bash
# After farm stack is running, add Heltec device access
sudo /opt/farm/edge/pi/utils/install_meshtastic_relay.sh
```

This adds WiFi hotspot functionality for direct Heltec device access.

## 📋 Service Overview

### Core Farm Stack
- **Purpose**: Complete farm monitoring and automation
- **Deployment**: Via Coolify from `docker-compose.yml`
- **Access**: Through Tailscale VPN
- **Data Flow**: Sensors → MQTT → Node-RED → InfluxDB → Dashboards

### Custom Go Services
Custom edge services are built from the Go source in the `src` directory. These handle:
- Serial communication with RS-485 sensors
- Bridge between farm protocols and MQTT
- Local data processing and buffering

For custom service development, see the [Raspberry Pi Setup documentation](../../docs/README.md#5-raspberry-pi-setup).

## 🔌 Meshtastic Integration Options

Choose your Meshtastic integration approach based on your hardware and requirements:

### Option A: External Heltec Devices (Relay Setup)

**Purpose**: Access external Heltec devices via WiFi relay for configuration and development.

**Prerequisites**: Foundation setup + Farm stack deployed via Coolify

```bash
# Only run after steps 1 & 2 are complete
sudo /opt/farm/edge/pi/utils/install_meshtastic_relay.sh
```

**What it adds:**
- 📶 **WiFi Hotspot**: `MESHRELAY` / `awesome33` 
- 🌐 **Device Network**: `10.42.0.0/24` (Pi at `10.42.0.1`)
- 🔗 **Remote Access**: Heltec devices accessible via `http://10.42.0.x`

**Management:**
```bash
check-relay    # Status of all connected devices
reset-relay    # Reset networking if issues occur
```

### Option B: Pi with Meshtastic Hat (Direct Integration)

**Purpose**: Pi becomes a Meshtastic node itself using a LoRa hat, eliminating need for external gateway devices.

**Hardware Required**: Meshtastic-compatible LoRa hat (e.g., Waveshare SX1262, Adafruit RFM95W)

**Documentation**: See [Meshtastic Hat Setup Guide](docs/meshtastic-hat-setup.md)

**Benefits:**
- ✅ **Direct integration** - No WiFi relay needed
- ✅ **Lower latency** - Pi is directly on mesh network  
- ✅ **Simplified network** - No additional subnets
- ✅ **Better reliability** - Fewer wireless hops

**Trade-offs:**
- ❌ **Hardware dependency** - Requires compatible hat (~$30-60)
- ❌ **Fixed position** - Pi location determines gateway coverage
- ❌ **Single point of failure** - Pi down = gateway down
- ❌ **Meshtastic is remotely accessible ONLY via CLI** - no web interface


## 🏗️ Architecture Options

### Option A: External Heltec Devices (Relay Setup)
```
Internet ────┐
             │ Tailscale VPN
             │
    ┌────────▼────────┐
    │ Raspberry Pi    │
    │ ┌─────────────┐ │
    │ │   Coolify   │ │ ◄─── Management Dashboard (port 8000)
    │ └─────────────┘ │
    │ ┌─────────────┐ │
    │ │ Farm Stack  │ │ ◄─── Mosquitto (1883), Node-RED (1880)
    │ │ Containers  │ │      InfluxDB (8086)
    │ └─────────────┘ │      Optional: ThingsBoard (8080)
    │ ┌─────────────┐ │
    │ │ WiFi Relay  │ │ ◄─── Hotspot (MESHRELAY)
    │ │ (Optional)  │ │      Network: 10.42.0.0/24
    │ └─────────────┘ │
    └─────────────────┘
             │
    ┌────────▼────────┐
    │ Heltec Gateway  │ ◄─── 10.42.0.2 (WiFi connected)
    │ Device          │      HTTP/Meshtastic access
    └─────────────────┘
             │
    ┌────────▼────────┐
    │ LoRa Mesh       │ ◄─── Field sensors, remote nodes
    │ Network         │      Data flows to gateway → MQTT
    └─────────────────┘
```

### Option B: Pi with Meshtastic Hat (Direct Integration)
```
Internet ────┐
             │ Tailscale VPN
             │
    ┌────────▼────────┐
    │ Raspberry Pi    │
    │ ┌─────────────┐ │
    │ │   Coolify   │ │ ◄─── Management Dashboard (port 8000)
    │ └─────────────┘ │
    │ ┌─────────────┐ │
    │ │ Farm Stack  │ │ ◄─── Mosquitto (1883), Node-RED (1880)
    │ │ Containers  │ │      InfluxDB (8086)
    │ └─────────────┘ │      Optional: ThingsBoard (8080)
    │ ┌─────────────┐ │
    │ │ Meshtastic  │ │ ◄─── Direct MQTT integration
    │ │ Service     │ │      Local device access
    │ └─────────────┘ │
    │ [LoRa Hat SPI]  │ ◄─── SX1262/RFM95W hardware
    └─────────────────┘
             │ (Direct LoRa)
    ┌────────▼────────┐
    │ LoRa Mesh       │ ◄─── Field sensors, remote nodes
    │ Network         │      Direct data flow to Pi MQTT
    └─────────────────┘
```

## 🔧 Service Access

| Service | Local Access | Tailscale Access | Purpose |
|---------|--------------|------------------|----------|
| **Coolify** | `http://PI_IP:8000` | `http://TS_IP:8000` | Container management |
| **Node-RED** | `http://PI_IP:1880` | `http://TS_IP:1880` | Automation & dashboards |
| **InfluxDB** | `http://PI_IP:8086` | `http://TS_IP:8086` | Database admin |
| **Mosquitto** | `PI_IP:1883` | `TS_IP:1883` | MQTT broker |
| **ThingsBoard** | `http://PI_IP:8080` | `http://TS_IP:8080` | IoT platform (if enabled) |
| **Heltec Devices** | N/A | `http://10.42.0.x` | Device web interface |

## 🔄 Data Flow Options

### Option A: External Heltec Gateway
1. **Field Sensors** → LoRa Mesh → **Heltec Gateway** → WiFi → **Pi Mosquitto MQTT**
2. **MQTT** → **Node-RED** → Processing/Automation → **InfluxDB**  
3. **Node-RED Dashboard** → Real-time visualization and controls
4. **Remote Access** → **Tailscale** → All services + Heltec devices (10.42.0.x)

### Option B: Pi with Meshtastic Hat
1. **Field Sensors** → LoRa Mesh → **Pi LoRa Hat** → **Local Meshtastic Service** → **Local MQTT**
2. **MQTT** → **Node-RED** → Processing/Automation → **InfluxDB**
3. **Node-RED Dashboard** → Real-time visualization and controls  
4. **Remote Access** → **Tailscale** → All services (no separate device IPs needed)

## 🛠️ Maintenance & Troubleshooting

### Common Commands
```bash
# Check all services
docker ps

# View service logs
docker logs farm-mosquitto
docker logs farm-nodered

# Restart services (via Coolify preferred)
docker-compose down && docker-compose up -d

# Check Meshtastic relay
check-relay

# Reset networking issues
reset-relay
```

### Troubleshooting Guide

| Issue | Solution |
|-------|----------|
| Can't access Coolify | Check Tailscale connection: `tailscale status` |
| Services won't start | Check disk space: `df -h` and service logs |
| MQTT connection fails | Verify Mosquitto container and port 1883 |
| **Option A: Heltec devices unreachable** | Run `reset-relay` and check route approval |
| **Option A: WiFi hotspot not working** | Uses NetworkManager (`nmcli`) - no hostapd needed |
| **Option B: LoRa hat not detected** | Check SPI enabled: `raspi-config` → Interface → SPI |
| **Option B: Meshtastic service fails** | Check hat connections and `meshtastic --info` |
| Data not flowing | Check Node-RED flows and MQTT topics |

## 🔒 Security Notes

- All external access is via **Tailscale VPN only**
- Default passwords **must be changed** in production
- Firewall blocks all public ports except Tailscale
- Service data stored in `/srv/` with proper permissions
- Regular backups configured via Coolify

## 📚 Next Steps

1. **Configure Node-RED flows** for your specific sensors
2. **Build Node-RED dashboards** using Dashboard v2 nodes
3. **Choose Meshtastic integration:**
   - **Option A**: Configure Heltec devices to connect to MESHRELAY WiFi
   - **Option B**: Install compatible LoRa hat and follow [Hat Setup Guide](docs/meshtastic-hat-setup.md)
4. **Set up alerting** via Node-RED notifications
5. **Implement backup strategies** for critical data

### Recommended Hardware Combinations

**For development/testing** (Option A):
- Raspberry Pi 4B (4GB) + External Heltec devices
- More flexible positioning, easier debugging

**For production deployment** (Option B):  
- Raspberry Pi 4B (4GB) + Waveshare SX1262 LoRa HAT
- More reliable, lower latency, simplified network
