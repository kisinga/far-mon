# edge/ — Farm Site Code

This directory contains all code and configuration for devices and services running on the farm itself.

## Subfolders

| Folder             | Device/Service        | Notes                                 |
|--------------------|----------------------|---------------------------------------|
| `pi/`              | Raspberry Pi         | ThingsBoard, relay bridge, utilities  |
| `heltec-remote/`   | Remote sensor node   | Minimal firmware, config-driven       |
| `heltec-relay/`    | LoRa<->serial relay  | Updatable, protocol evolves here      |

## Deployment Sequence

1. Flash/prepare firmware in `heltec-remote/` for remote sensor node(s).
2. Flash/prepare firmware in `heltec-relay/` for relay (LoRa<->serial) node.
3. Set up Pi per instructions in `pi/`, including Docker and systemd services.
4. Connect everything per `../docs/hardware.md`.

**To swap communication interface:**  
- Replace serial comms in Pi/relay bridge and relay node with LoRa Hat or alternative, leveraging shared interface definitions in `../../shared/`.
