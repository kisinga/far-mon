# edge/heltec-remote/ — Remote Sensor Node Firmware

## Purpose

- Reads data from attached sensors (Sunverter, tank, flow, environmental, etc.)
- Periodically transmits packed data via LoRa to relay node.
- Listens for config commands after each transmit window, writes to flash, and ACKs in next packet.

## Features

- Minimal code changes needed after deployment: all operational changes should be done by config.
- Protocol details in `../../shared/protocol.md`.
- Example configs in `../../docs/config-samples/remote-config.json`.

## To Update

- Flash via standard USB programmer.
- Config changes delivered via LoRa downlink through relay/Pi.

**Note:**  
Physical firmware update is *only* needed for new hardware or bug fixes.

# Heltec Remote Node Specification

This document describes the **Heltec WiFi LoRa 32 (V3) Remote Sensor Node** firmware abstractions, data structures, storage & boot flow, radio setup, and pseudocode. It is intended to be complete enough to drive a generic code generator.

---

## 1. Device Role & Responsibilities

- **Collect** data from:
  - Sunverter 4b via UART/RS485
  - Tank level sensor (analog/ultrasonic/etc.)
  - Four flow meters: borehole→tank, tank→domestic, tank→farm, domestic→sales
  - Optional environmental sensors (temperature, humidity, etc.)
- **Package** readings into a compact payload
- **Transmit** via LoRa at fixed intervals
- **Listen** for LoRa-downlink “config packets” and update:
  - Sensor table (pins, types, intervals, scales)
  - LoRa radio parameters
  - (Optional) WiFi/BLE credentials & params
- **Persist** config in flash (NVS)
- **ACK** config changes in next uplink
- **Support** programmatic reboot to apply new configs

---

## 2. Data Structures

### 2.1. SensorConfig

```cpp
// Defines one sensor “slot”
struct SensorConfig {
  uint8_t  slotId;        // 0..MAX_SLOTS-1
  uint8_t  pin;           // GPIO number
  uint8_t  type;          // 0=Digital,1=Analog,2=I2C,3=OneWire,...
  uint16_t pollInterval;  // seconds
  float    scale;         // multiplier for raw value
  bool     enabled;       // true=active
};
2.2. LoRaConfig
cpp
Copy
Edit
struct LoRaConfig {
  uint32_t freq;      // e.g. 868000000
  uint8_t  sf;        // 7..12
  uint32_t bw;        // Hz, e.g. 125000
  uint8_t  cr;        // 5..8 (4/5..4/8)
  uint16_t preamble;  // symbols
  int8_t   txPower;   // +2..+20 dBm
  uint8_t  syncWord;  // 0..255
};
2.3. (Optional) WiFiConfig & BLEConfig
cpp
Copy
Edit
struct WiFiConfig {
  char     ssid[33];
  char     psk[65];
  bool     stationMode;
  uint16_t retryInterval;
};

struct BLEConfig {
  char     deviceName[17];
  uint16_t advIntervalMs;
  uint8_t  advChannels;  // bitmask
  uint16_t connMin, connMax;
  bool     enabled;
};
2.4. Top-Level ConfigPacket
cpp
Copy
Edit
struct ConfigPacket {
  uint8_t      version;
  LoRaConfig   lora;
  WiFiConfig   wifi;       // optional
  BLEConfig    ble;        // optional
  uint8_t      numSensors;
  SensorConfig sensors[MAX_SLOTS];
  uint16_t     crc16;      // CRC over all prior bytes
};
3. Storage & Persistence
Use ESP32 NVS for flash-backed key/value:

prefs.begin("sensor") → getBytes("table") → load or fall back to defaults

prefs.putBytes("table", sensorTable, sizeof(sensorTable))

Atomicity: batch writes, verify CRC before commit

“pendingReboot” flag in NVS to trigger a restart after a config update

4. Boot & Init Flow
cpp
Copy
Edit
void setup() {
  Serial.begin(115200);
  initLoRa();            // enable SX1276 with defaults
  loadDefaultConfigs();  // pre-embedded defaults
  loadSavedConfigs();    // override from NVS if valid
  applyConfigs();        // pinMode(), Wire.begin(), LoRa.setX(), etc.
  if (checkPendingReboot()) ESP.restart();
  scheduleNextPoll();
}
applyConfigs() dynamically:

For each SensorConfig slot:

cpp
Copy
Edit
if (cfg.enabled) {
  switch(cfg.type) {
    case ANALOG:  pinMode(cfg.pin, INPUT);       break;
    case DIGITAL: pinMode(cfg.pin, INPUT_PULLUP);break;
    case I2C:     Wire.begin();                  break;
    // etc.
  }
  startPolling(slotId, cfg.pollInterval);
}
LoRa radio setup:

cpp
Copy
Edit
LoRa.setFrequency(loraConfig.freq);
LoRa.setSpreadingFactor(loraConfig.sf);
LoRa.setSignalBandwidth(loraConfig.bw);
LoRa.setCodingRate4(loraConfig.cr);
LoRa.setPreambleLength(loraConfig.preamble);
LoRa.setTxPower(loraConfig.txPower);
LoRa.setSyncWord(loraConfig.syncWord);
5. Main Loop & Pseudocode
cpp
Copy
Edit
void loop() {
  // 1. Poll sensors on schedule
  for each slot in sensorTable:
    if (timeToPoll(slot)) {
      value = readSensor(slot);
      scaled = value * slot.scale;
      storeUplinkData(slot.slotId, scaled);
    }

  // 2. When uplink window:
  if (uplinkTime()) {
    packet = packUplinkData();         // include configVersion
    LoRa.beginPacket();
      LoRa.write(packet, packetLen);
    LoRa.endPacket();

    // 3. Open RX window for config
    if (LoRa.parsePacket(timeoutMs)) {
      incoming = readConfigPacket();
      if (validateCRC(incoming)) {
        applyNewConfig(incoming);
        saveConfigToNVS(incoming);
        setPendingReboot();           // optional
      }
    }
  }

  // 4. Background tasks
  handleNVMSTasks();
  yield();  // ESP32 WiFi, background upkeep
}
6. Config Update & ACK
applyNewConfig() merges incoming.sensors[], incoming.lora, etc.

saveConfigToNVS() writes new tables to flash

ACK: include configVersion field in next uplink packet so relay/Pi knows it applied

7. Programmatic Reboot
cpp
Copy
Edit
// After saving config and before ending setup or in loop:
if (checkPendingReboot()) {
  prefs.begin("system", false);
  prefs.remove("pendingReboot");
  prefs.end();
  ESP.restart();    // or esp_restart();
}
8. Error Handling & Validation
CRC-16 on all config packets

Version field in ConfigPacket for forward compatibility

Fallback to defaults on corrupt data

Flash wear: minimize writes; only update changed entries

9. Extensibility
Add new sensor types by extending type enum and readSensor() switch

Swap transport by isolating LoRa calls behind ISensorComm interface

Future WiFi/BLE support via WiFiConfig/BLEConfig blocks

