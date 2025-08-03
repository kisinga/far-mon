# 3. Firmware

This document provides detailed information about the firmware for both the remote and relay nodes.

## 3.1. Remote Sensor Node (`heltec-remote`)

The remote node's firmware is designed for minimal power consumption and reliable, config-driven operation.

### Responsibilities

*   **Collect data** from attached sensors (Sunverter, tank level, flow meters, etc.).
*   **Package readings** into a compact payload for LoRa transmission.
*   **Transmit** data at fixed intervals.
*   **Listen** for downlink configuration packets to update settings.
*   **Persist** configuration in flash memory.
*   **Acknowledge** configuration changes in the next uplink.

### Data Structures

The firmware uses a set of C++ structs to manage configuration.

#### `SensorConfig`

Defines a single sensor "slot."

```cpp
struct SensorConfig {
  uint8_t  slotId;        // 0..MAX_SLOTS-1
  uint8_t  pin;           // GPIO number
  uint8_t  type;          // 0=Digital, 1=Analog, 2=I2C, 3=OneWire,...
  uint16_t pollInterval;  // seconds
  float    scale;         // multiplier for raw value
  bool     enabled;       // true=active
};
```

#### `LoRaConfig`

Manages LoRa radio parameters.

```cpp
struct LoRaConfig {
  uint32_t freq;      // e.g. 868000000
  uint8_t  sf;        // 7..12
  uint32_t bw;        // Hz, e.g. 125000
  uint8_t  cr;        // 5..8 (4/5..4/8)
  uint16_t preamble;  // symbols
  int8_t   txPower;   // +2..+20 dBm
  uint8_t  syncWord;  // 0..255
};
```

#### `ConfigPacket`

The top-level configuration packet.

```cpp
struct ConfigPacket {
  uint8_t      version;
  LoRaConfig   lora;
  WiFiConfig   wifi;       // optional
  BLEConfig    ble;        // optional
  uint8_t      numSensors;
  SensorConfig sensors[MAX_SLOTS];
  uint16_t     crc16;      // CRC over all prior bytes
};
```

### Storage and Persistence

Configuration is stored in the ESP32's Non-Volatile Storage (NVS).

*   Configurations are loaded at boot.
*   New configurations received via LoRa are written to NVS.
*   Writes are batched to minimize flash wear.
*   A `pendingReboot` flag can be set in NVS to trigger a restart after a config update.

### Boot and Initialization Flow

```cpp
void setup() {
  Serial.begin(115200);
  initLoRa();
  loadDefaultConfigs();
  loadSavedConfigs();    // Override from NVS if valid
  applyConfigs();
  if (checkPendingReboot()) ESP.restart();
  scheduleNextPoll();
}
```

### Main Loop Pseudocode

```cpp
void loop() {
  // 1. Poll sensors on schedule
  for each slot in sensorTable:
    if (timeToPoll(slot)) {
      value = readSensor(slot);
      scaled = value * slot.scale;
      storeUplinkData(slot.slotId, scaled);
    }

  // 2. Transmit when uplink window is open
  if (uplinkTime()) {
    packet = packUplinkData();
    LoRa.beginPacket();
    LoRa.write(packet, packetLen);
    LoRa.endPacket();

    // 3. Open RX window for config download
    if (LoRa.parsePacket(timeoutMs)) {
      incoming = readConfigPacket();
      if (validateCRC(incoming)) {
        applyNewConfig(incoming);
        saveConfigToNVS(incoming);
        setPendingReboot(); // Optional
      }
    }
  }

  // 4. Handle background tasks
  handleNVMSTasks();
  yield();
}
```

## 3.2. Relay Node (`heltec-relay`)

The relay node acts as a bridge between the LoRa network and the Raspberry Pi.

### Responsibilities

*   Receives sensor data from remote nodes via LoRa.
*   Forwards data to the Pi via serial.
*   Receives configuration commands from the Pi and pushes them to remote nodes via LoRa.
*   Handles acknowledgments from remote nodes and reports status upstream.

The relay firmware is simpler than the remote node's, as most of the logic is handled by the `relay-bridge` service on the Raspberry Pi.
