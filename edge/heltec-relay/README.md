# edge/heltec-relay/ — Relay Node Firmware

## Purpose

- Receives sensor data from remote node(s) via LoRa.
- Forwards data to Pi via serial (or other, via interface).
- Receives config commands from Pi (serial), pushes to remote(s) via LoRa.
- Handles ACK from remote and reports status upstream.

## Extensibility

- Serial interface is swappable: can support SPI/I2C or direct LoRa in future.
- Updatable over USB.

## Protocol

- Message format: see `../../shared/protocol.md`.

## Setup

- Flash via USB.
- Configure device IDs and channels in `config.h` or via initial serial setup.

**Logs and troubleshooting tips:**  
See `../docs/troubleshooting.md` (if present).
