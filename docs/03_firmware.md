# 3. Firmware

This document provides a high-level overview of the firmware for the remote and relay nodes. For detailed implementation information, refer to the `README.md` files in the respective edge device directories.

## 3.1. Remote Sensor Node (`heltec-remote`)

The remote node's firmware is designed for minimal power consumption and reliable, config-driven operation. It collects data from sensors and transmits it to the relay node.

For more details, see the [heltec-remote documentation](../edge/heltec-remote/README.md).

## 3.2. Relay Node (`heltec-relay`)

The relay node acts as a bridge between the LoRa network and the Raspberry Pi. It forwards data between the two and handles configuration updates.

For more details, see the [heltec-relay documentation](../edge/heltec-relay/README.md).
