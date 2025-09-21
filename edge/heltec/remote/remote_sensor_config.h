// Remote Sensor Configuration
// Remote-specific sensor system configuration

#pragma once

#include <stdint.h>

// Dummy struct for now
struct RemoteSensorConfig {
    bool enableSensorSystem = false;
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
