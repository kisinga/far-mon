#pragma once

#include "lib/config.h"
#include "remote_sensor_config.h"

// Per-device configuration for Remote
static constexpr const char* REMOTE_DEVICE_ID = "03";

inline RemoteConfig buildRemoteConfig() {
    RemoteConfig cfg = RemoteConfig::create(REMOTE_DEVICE_ID);
    // Remotes usually operate over LoRa only; WiFi off by default
    cfg.communication.wifi.enableWifi = false;

    // Configure analog sensor pin (ADC1_CH6 = GPIO34 on Heltec LoRa 32 V3)
    // Use board-defined safe ADC pin for ESP32-S3 (Heltec V3): GPIO1
    cfg.analogInputPin = BATTERY_ADC_PIN;

    // Example: adjust telemetry/report intervals if needed
    // cfg.telemetryReportIntervalMs = 2000;

    return cfg;
}

inline RemoteSensorConfig buildRemoteSensorConfig() {
    RemoteSensorConfig sensorCfg = createRemoteSensorConfig();

    // Configure sensors for this remote device
    // Enable sensors as needed - adjust pins and timing as required

    // Example configuration - uncomment to enable sensors:

    // Enable ultrasonic sensor for distance measurement (e.g., water depth)
    sensorCfg.enableUltrasonic(true, 12, 13);  // trig=12, echo=13, read every 30s

    // Enable water level sensor (float switch)
    sensorCfg.enableWaterLevel(true, 14);      // pin=14, read every 10s

    // Enable water flow sensor (YF-G1)
    sensorCfg.enableWaterFlow(true, 15);       // pin=15, read every 1s

    // Enable temperature/humidity sensor (DHT-style)
    sensorCfg.enableTempHumidity(true, 18);    // data=18, read every 2s

    // Note: RS485 sensor requires additional hardware setup
    // sensorCfg.enableRS485(true, 16, 17);    // RE=16, DE=17

    // Set batch interval - how often to send collected readings (default is 60000ms = 1 minute)
    // sensorCfg.setBatchInterval(30000); // 30 seconds for more frequent updates

    // Individual sensor timing can be customized:
    // sensorCfg.ultrasonicConfig.readIntervalMs = 15000;  // 15 seconds
    // sensorCfg.waterLevelConfig.readIntervalMs = 5000;   // 5 seconds

    return sensorCfg;
}


