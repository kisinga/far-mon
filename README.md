# Farm Infra ⟁

This repository contains all source code, configuration, and documentation for the autonomous farm monitoring and control system.

**High-Level Structure:**
- Modular design: each device and logical function is isolated and swappable.
- KISS, DRY, SOLID principles at every layer.
- Designed for low-maintenance, high-resilience edge deployment, with future-proof extensibility.

## Directory Overview

| Folder          | Purpose                                       |
|-----------------|-----------------------------------------------|
| `docs/`         | Wiring diagrams, protocols, and config docs   |
| `edge/`         | Code for farm site (remote, relay, Pi, etc)   |
| `core/`         | Cloud/home-side code (future/optional)        |
| `shared/`       | Protocols, config schemas, serialization libs |

**Quickstart:**  
1. See [`docs/README.md`](docs/README.md) for system diagram and hardware map.
2. For edge deployment, start at [`edge/README.md`](edge/README.md).

---

1. System Topology Diagram  
``` mermaid
graph LR
  subgraph Field Site
    RemoteSensor1((Heltec LoRa Node #1))
    RemoteSensor2((Heltec LoRa Node #2))
    RelayNode((Heltec Relay Node))
    Pi((Raspberry Pi Edge Server))
    Router((WiFi Router))
  end
  subgraph Home/Cloud
    CoreServer((Central Server - future))
  end

  RemoteSensor1 -- LoRa --> RelayNode
  RemoteSensor2 -- LoRa --> RelayNode
  RelayNode -- Serial/USB --> Pi
  Pi -- Ethernet/WiFi --> Router
  Router -- Tailscale VPN --> CoreServer
  Pi -- Tailscale VPN --> CoreServer
  Pi -. SSH/HTTP(S)/VPN .-> User[(Authorized User)]
  Pi -. WiFi .-> RelayNode
```
  
2. Layered Architecture  
A. Physical Layer
Component	Details
Remote Heltec	WiFi LoRa 32(V3), remote field
Relay Heltec	WiFi LoRa 32(V3), near Pi
Raspberry Pi	Model 3/4, runs edge services
WiFi Router	Local networking at farm site
Sensors	Sunverter 4b (RS485/Serial), water flow/level, env sensors
Power	Mains/solar as per site

B. Data & Communication Layer
Path	Protocol	Details
Remote Heltec → Relay Heltec	LoRa (Semtech)	Uplink sensor data, 433/868/915 MHz as region
Relay Heltec → Pi	Serial/USB UART	Binary/text packets; bidirectional for config
Pi → ThingsBoard (internal)	MQTT/HTTP	Standard TB device telemetry API
Pi → User	HTTP(S), Tailscale VPN	Dashboard, SSH admin, VPN-guarded
Pi ↔ CoreServer (future)	VPN/MQTT/HTTP	For ETL, sync, centralized analytics

C. Logical/Software Layer
Layer/Module	Description
Remote Firmware	Minimal; sensor reads, data packing, LoRa TX, config RX
Relay Firmware	LoRa RX/TX, serial bridge, config handler, ACK logic
Pi Edge Stack	Dockerized ThingsBoard, relay-bridge service (serial<->MQTT/HTTP), VPN, logging
Shared Protocols	All nodes use same serialization/packing from /shared
User Interface	ThingsBoard dashboards, config widgets, SSH access via VPN
Future Core	Central analytics, sync, long-term backup (optional)

3. Information Flow
A. Sensor Data Path (Uplink)
Remote Heltec

Reads all attached sensors (Sunverter 4b, tank, flow, env).

Packs readings into fixed-format binary or compressed text.

Transmits over LoRa to relay node at set interval.

Relay Heltec

Listens for LoRa packets.

Forwards sensor packets to Pi via USB serial (can buffer if link is busy).

Receives config packets via serial (from Pi), sends to remote node via LoRa.

Raspberry Pi (Edge Server)

Runs relay-bridge service:

Reads serial data, validates, parses, and forwards to ThingsBoard (MQTT or HTTP telemetry API).

Runs local ThingsBoard in Docker:

Device tokens registered for each remote.

Stores, visualizes, and rules on incoming telemetry.

Logs all traffic, handles errors, and provides local backup.

User

Connects via Tailscale VPN for dashboard (HTTP), config, and admin (SSH).

B. Configuration & Downlink Path
User/ThingsBoard UI

Sends configuration changes (e.g., sample rate, sensor enable, thresholds) via dashboard RPC, attribute update, or custom widget.

ThingsBoard → Pi relay-bridge

Via MQTT (subscribe to device topic) or REST API, Pi relay-bridge receives config update command.

Pi relay-bridge → Relay Heltec

Encodes config packet, sends via serial to relay node.

Relay Heltec → Remote Heltec

Broadcasts config packet over LoRa.

Waits for ACK (from next uplink or immediate).

Remote Heltec

Listens for config after each transmit.

If config received: validates, stores to flash, ACKs.

Applies on next cycle.

ACK Path

Remote includes config version or ACK field in next data uplink.

Pi updates ThingsBoard/UI with status.

4. Protocols & Formats
Link	Format	Notes
Remote → Relay	Binary/compact	Timestamp, sensor ID, readings, CRC
Relay ↔ Pi (Serial)	Binary/JSON/CSV	Pluggable for future interface
Config Downlink	Binary/JSON	Device ID, version, config fields, CRC
Pi ↔ ThingsBoard	JSON over MQTT/HTTP	Uses standard TB API

All packing/unpacking logic centralized in /shared/ for single source of truth.

5. Physical Deployment Map
Remote Heltec Node(s):

Placed at sensor cluster points (e.g., by borehole, tank, field).

Powered by local solar or battery.

LoRa antenna sized for field coverage (aim for line-of-sight; min. mast height as needed).

Relay Heltec Node:

Near Pi, inside comms room or farmhouse.

USB tethered to Pi.

Sited for best LoRa RX/TX coverage.

Raspberry Pi:

Indoors, on UPS/battery for resilience.

Ethernet or WiFi to router.

Cooled and protected from dust.

WiFi Router:

Connects Pi and user devices.

Serves as local AP for field engineers if needed.

Power:

Main site solar+backup or mains+UPS as available.

6. Security Model
Tailscale VPN:

All remote access (dashboard, SSH, SFTP, etc) is only via VPN.

No direct WAN access open.

Physical security:

Pi and relay node indoors/locked; remotes tamper-evident if possible.

7. Maintenance & Extensibility
Adding new sensors:

Update remote config via LoRa (no firmware flash unless new hardware class).

Relay upgrades:

Swap serial for LoRa Hat on Pi by changing bridge interface.

More remotes:

Unique device ID/address in protocol; scalable to N nodes with same base firmware.

Data sync/backup:

Scheduled backup/export from Pi to home server (via Tailscale, SFTP, or sync script).

8. Critical Dependencies
Layer	What Breaks if Fails	Recovery Plan
Remote Node	No data from sensors	Manual visit (rare)
Relay Node	No LoRa uplink/downlink	Replace, or bypass with Pi+LoRa Hat (future)
Pi	No data collection/dash	SD card image restore, spare Pi ready
WiFi Router	No dashboard/SSH/VPN	Spare/backup recommended
Tailscale	No remote access	Local-only, fix WAN ASAP

9. Reference Standards/Best Practices
LoRa radio use per Semtech SX127x family datasheet.

Device telemetry via ThingsBoard MQTT API.

Secure VPN via Tailscale.

All data at rest is local; all remote access by VPN only.

10. System Flow Summary
less
Copy
Edit
[Sensor/Remote Heltec] 
    |--(LoRa: data uplink, config downlink)--> 
[Relay Heltec] 
    |--(Serial: bidirectional data/config)--> 
[Raspberry Pi (Edge)] 
    |--(MQTT/HTTP, VPN protected)--> 
[ThingsBoard dashboard, user, future home server]
Appendix: Expansion Paths
Add Pi LoRa Hat: swap relay for direct gateway mode.

Add cellular or satellite fallback to Pi.

Integrate with additional field devices (irrigation, weather, etc) by extending protocol and remote firmware/config.

For wiring diagrams, pinouts, and board-level detail, see docs/hardware.md.
For protocol details and examples, see docs/protocols.md and shared/.