#pragma once

#include "lib/config.h"

// Per-device configuration for Remote
static constexpr const char* REMOTE_DEVICE_ID = "03";

inline RemoteConfig buildRemoteConfig() {
    RemoteConfig cfg = RemoteConfig::create(REMOTE_DEVICE_ID);
    // Remotes usually operate over LoRa only; WiFi off by default
    cfg.communication.wifi.enableWifi = false;

    // Configure analog sensor pin (ADC1_CH6 = GPIO34 on Heltec LoRa 32 V3)
    cfg.analogInputPin = 34;

    // Example: adjust telemetry/report intervals if needed
    // cfg.telemetryReportIntervalMs = 2000;

    return cfg;
}


