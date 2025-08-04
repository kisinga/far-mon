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

## Firmware

This Arduino sketch is designed for a Heltec LoRa 32 device that acts as a relay.

### Features

- **Dynamic Configuration**: Manages settings using a `config.json` file stored in the device's LittleFS flash memory.
- **First-Run Setup**: If no configuration file is found on boot, a new one is created with default settings.
- **Extensible**: The configuration system is built to be easily extendable with new settings in the future.
- **Debug Mode**: Includes a `debugMode` flag in the configuration to simulate success scenarios for testing without needing full hardware setup.

### Default Configuration

On the first run, `config.json` will be created with:
```json
{
  "debugMode": true,
  "usbBaudRate": 115200
}
```

### Dependencies

This project relies on the following Arduino libraries:
- `ArduinoJson`: For handling JSON serialization and deserialization.
- `LittleFS`: For file system management on the ESP32.

You will need to install these libraries through the Arduino IDE's Library Manager.

### How to Use

1.  Open the `heltec-relay/heltec-relay.ino` sketch in the Arduino IDE.
2.  Install the required libraries from the Library Manager.
3.  Select the correct board (Heltec LoRa 32) and port.
4.  Upload the sketch to your device.
5.  Open the Serial Monitor to view the output. The default baud rate for initial messages is 115200. The sketch will re-initialize the serial port based on the `usbBaudRate` in the `config.json` file.
