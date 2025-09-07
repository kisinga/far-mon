#pragma once

#include "../lib/config.h"

// Per-device configuration for Remote
static constexpr const char* REMOTE_DEVICE_ID = "03";

inline RemoteConfig buildRemoteConfig() {
    RemoteConfig cfg = RemoteConfig::create(REMOTE_DEVICE_ID);
    // Remotes usually operate over LoRa only; WiFi off by default
    cfg.communication.wifi.enableWifi = false;

    // Example: adjust telemetry/report intervals if needed
    // cfg.telemetryReportIntervalMs = 2000;

    return cfg;
}


