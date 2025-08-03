# Raspberry Pi

This directory contains the code and configuration for the Raspberry Pi edge stack.

## Roles

*   Runs ThingsBoard for the local dashboard, data storage, and rule engine.
*   Runs the `relay-bridge` to move data between the relay node and ThingsBoard.
*   Provides a VPN entry point (Tailscale) for secure remote access.

## Contents

| Folder          | Description                                  |
| :-------------- | :------------------------------------------- |
| `thingsboard/`  | Setup and configuration for ThingsBoard.     |
| `relay-bridge/` | Code for the serial/LoRa-to-MQTT bridge.     |
| `utils/`        | Scripts for logging, backup, and self-tests. |

For detailed setup and configuration instructions, see the [Raspberry Pi Setup documentation](https://github.com/ryanjyoder/farm/blob/main/docs/05_raspberry_pi.md).
