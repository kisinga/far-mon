#pragma once

#include "../lib/device_config.h"

struct RemoteConfig : DeviceConfig {
    bool enableAnalogSensor;
    uint8_t analogInputPin;
    uint32_t analogReadIntervalMs;
    uint32_t telemetryReportIntervalMs;
    float analogReferenceVoltage;
    uint8_t masterNodeId;

    RemoteConfig() {
        deviceType = DeviceType::Remote;
        heartbeatIntervalMs = 1000;
        enableDisplay = true;
        enableDebug = true;
        enableAnalogSensor = true;
        displayUpdateIntervalMs = 200;
        loraTaskIntervalMs = 50;
        analogInputPin = 34; // Default for Heltec LoRa 32 V3
        analogReadIntervalMs = 200;
        telemetryReportIntervalMs = 2000;
        analogReferenceVoltage = 3.30f;
        masterNodeId = 1;
    }
};

// Factory
RemoteConfig createRemoteConfig(const char* deviceId);


