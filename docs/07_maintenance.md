# 7. Maintenance and Extensibility

This document provides guidance on maintaining and extending the farm monitoring system.

## 7.1. Maintenance

### Critical Dependencies

| Layer         | What Breaks if It Fails       | Recovery Plan                                     |
| :------------ | :---------------------------- | :------------------------------------------------ |
| Remote Node   | No data from sensors          | Manual visit for repair or replacement (rare).    |
| Relay Node    | No LoRa uplink/downlink       | Replace, or bypass with a Pi LoRa Hat.            |
| Raspberry Pi  | No data collection/dashboard  | Restore from an SD card image; keep a spare Pi on hand. |
| WiFi Router   | No dashboard/SSH/VPN access   | A spare/backup router is recommended.             |
| Tailscale     | No remote access              | System runs in local-only mode; fix WAN access ASAP. |

### Data Backup and Sync

*   Scheduled backups and exports from the Raspberry Pi to a home server can be configured via Tailscale, SFTP, or a custom sync script.

## 7.2. Extensibility

### Adding New Sensors

*   Update the remote node's configuration via LoRa downlink. No firmware flashing is required unless a new hardware class is being introduced.

### Adding More Remote Nodes

*   The system is scalable to multiple nodes. Each remote node must have a unique device ID/address, which is defined in the protocol.

### Swapping the Communication Interface

*   The `relay-bridge` service on the Pi can be modified to support different communication interfaces, such as a LoRa Hat, by updating the comms class.

### Expansion Paths

*   **Pi LoRa Hat**: The relay node can be replaced with a LoRa Hat on the Pi for a more direct gateway.
*   **Cellular/Satellite Fallback**: A cellular or satellite modem can be added to the Pi for WAN redundancy.
*   **Additional Devices**: The system can be integrated with other field devices (e.g., irrigation systems, weather stations) by extending the protocol and firmware.
