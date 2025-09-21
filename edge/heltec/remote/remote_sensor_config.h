// Remote Sensor Configuration
// Remote-specific sensor system configuration

#pragma once

#include <stdint.h>

// Remote Sensor Configuration
struct RemoteSensorConfig {
    bool enableSensorSystem = true; // Enable for debug sensors

    // Debug sensors (enabled by default for testing)
    struct {
        bool enabled = true;
    } temperatureConfig;
    struct {
        bool enabled = true;
    } humidityConfig;
    struct {
        bool enabled = true;
    } batteryConfig;

    // Real sensors (disabled by default)
    struct {
        bool enabled = false;
    } ultrasonicConfig;
    struct {
        bool enabled = false;
    } waterLevelConfig;
    struct {
        bool enabled = false;
    } waterFlowConfig;
    struct {
        bool enabled = false;
    } rs485Config;
    struct {
        bool enabled = false;
    } tempHumidityConfig;

    struct {
        uint8_t ultrasonicTrig = 0;
        uint8_t ultrasonicEcho = 0;
        uint8_t waterLevel = 0;
        uint8_t waterFlow = 0;
        uint8_t rs485RE = 0;
        uint8_t rs485DE = 0;
        uint8_t tempHumidity = 0;
    } pins;
};
