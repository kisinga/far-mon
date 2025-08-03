# edge/pi/ — Raspberry Pi Edge Stack

## Roles

- Runs ThingsBoard (local dashboard, data store, rule engine)
- Runs `relay-bridge/` to move data between relay node (serial or LoRa) and ThingsBoard (via MQTT/HTTP)
- Provides VPN entry (Tailscale) for secure, remote dashboard/SSH access
- Manages all field logging, config distribution, and service health

## Contents

| Folder          | Description                                         |
|-----------------|-----------------------------------------------------|
| `thingsboard/`  | Setup, configs, and Docker Compose for ThingsBoard  |
| `relay-bridge/` | All code for serial/LoRa<->MQTT bridge, config push |
| `utils/`        | Scripts for logging, backup, or self-tests          |

**Extending:**  
- Swap in LoRa Hat by replacing comms class in `relay-bridge/`.
- Add additional dashboard or monitoring tools as needed.

See `../../docs/overview.md` for network and physical diagrams.
