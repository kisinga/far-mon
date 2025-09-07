#pragma once

#include "../lib/device_config.h"

struct RelayConfig : DeviceConfig {
    bool enableWifi;
    uint32_t wifiTaskIntervalMs;
    uint32_t peerMonitorIntervalMs;
    uint8_t maxPeers;

    RelayConfig() {
        deviceType = DeviceType::Relay;
        heartbeatIntervalMs = 1000;
        enableDisplay = true;
        enableDebug = true;
        enableWifi = true;
        displayUpdateIntervalMs = 800;
        loraTaskIntervalMs = 50;
        wifiTaskIntervalMs = 100;
        peerMonitorIntervalMs = 2000;
        maxPeers = 16;
    }
};

// Factory
RelayConfig createRelayConfig(const char* deviceId);


