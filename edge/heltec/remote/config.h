#pragma once

#include "lib/core_config.h"
#include "remote_sensor_config.h"

#define BATTERY_ADC_PIN 1

// Per-device configuration for Remote
inline RemoteConfig buildRemoteConfig() {
    RemoteConfig cfg = RemoteConfig::create(3); // Example ID 3
    // Remotes usually operate over LoRa only; WiFi off by default
    cfg.communication.wifi.enableWifi = false;

    // Use board-defined safe ADC pin for ESP32-S3 (Heltec V3): GPIO1
    cfg.battery.adcPin = BATTERY_ADC_PIN;

    return cfg;
}

inline RemoteSensorConfig buildRemoteSensorConfig() {
    RemoteSensorConfig cfg{};
    cfg.enableSensorSystem = true; 
    return cfg;
}


