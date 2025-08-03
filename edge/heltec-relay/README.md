# Heltec Relay Node

This directory contains the firmware for the Heltec relay node.

## Purpose

The relay node receives sensor data from remote nodes via LoRa and forwards it to the Raspberry Pi via serial. It also relays configuration commands from the Pi to the remote nodes.

## Implementation Details

The relay node acts as a bridge between the LoRa network and the Raspberry Pi. While it is simpler than the remote node's firmware, it plays a critical role in the system's data pipeline.

### Responsibilities

*   **LoRa Uplink**: Receives sensor data from all remote nodes via LoRa.
*   **Serial Forwarding**: Forwards the received LoRa packets to the Raspberry Pi via the serial interface.
*   **Serial Downlink**: Receives configuration commands from the Pi via the serial interface.
*   **LoRa Downlink**: Broadcasts the configuration commands to the remote nodes via LoRa.
*   **Status Reporting**: Handles acknowledgments from remote nodes and reports their status upstream to the Pi.

### Main Loop Pseudocode

```cpp
void loop() {
  // 1. Check for incoming LoRa packets (uplink)
  if (LoRa.parsePacket()) {
    loraPacket = readLoRaPacket();
    // Forward the packet to the Pi via serial
    Serial.write(loraPacket, loraPacket.length);
  }

  // 2. Check for incoming serial data (downlink)
  if (Serial.available() > 0) {
    serialPacket = readSerialPacket();
    // Broadcast the packet to remote nodes via LoRa
    LoRa.beginPacket();
    LoRa.write(serialPacket, serialPacket.length);
    LoRa.endPacket();
  }

  // 3. Handle background tasks (e.g., status LEDs)
  updateStatusIndicators();
  yield();
}
```
