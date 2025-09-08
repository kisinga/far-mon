// Remote Sensor Configuration
// Remote-specific sensor system configuration

#pragma once

#include "sensor_interface.h"

// Remote sensor system configuration
struct RemoteSensorConfig {
    bool enableSensorSystem;
    uint32_t sensorBatchIntervalMs;  // Default batch interval for all sensors
    uint8_t maxSensors;              // Maximum number of sensors

    // Default sensor configurations
    SensorConfig ultrasonicConfig;
    SensorConfig waterLevelConfig;
    SensorConfig waterFlowConfig;
    SensorConfig rs485Config;
    SensorConfig tempHumidityConfig;

    // Pin configurations for sensors
    struct {
        uint8_t ultrasonicTrig;
        uint8_t ultrasonicEcho;
        uint8_t waterLevel;
        uint8_t waterFlow;
        uint8_t rs485RE;      // RS485 Receive Enable
        uint8_t rs485DE;      // RS485 Driver Enable
        uint8_t tempHumidity; // DHT data pin
    } pins;

    RemoteSensorConfig() :
        enableSensorSystem(true),
        sensorBatchIntervalMs(60000), // 1 minute default
        maxSensors(8),
        ultrasonicConfig("distance", 30000, 60000, false, 1), // 30s read, 1min batch
        waterLevelConfig("water_level", 10000, 60000, false, 2), // 10s read, 1min batch
        waterFlowConfig("flow_rate", 1000, 60000, false, 3), // 1s read, 1min batch
        rs485Config("rs485", 5000, 60000, false, 4), // 5s read, 1min batch
        tempHumidityConfig("temp_humidity", 2000, 60000, false, 5) // 2s read, 1min batch
    {
        // Default pin assignments (can be overridden)
        pins.ultrasonicTrig = 12;
        pins.ultrasonicEcho = 13;
        pins.waterLevel = 14;
        pins.waterFlow = 15;
        pins.rs485RE = 16;
        pins.rs485DE = 17;
        pins.tempHumidity = 18;
    }

    // Enable specific sensors
    void enableUltrasonic(bool enable = true, uint8_t trigPin = 12, uint8_t echoPin = 13) {
        ultrasonicConfig.enabled = enable;
        pins.ultrasonicTrig = trigPin;
        pins.ultrasonicEcho = echoPin;
    }

    void enableWaterLevel(bool enable = true, uint8_t sensorPin = 14) {
        waterLevelConfig.enabled = enable;
        pins.waterLevel = sensorPin;
    }

    void enableWaterFlow(bool enable = true, uint8_t sensorPin = 15) {
        waterFlowConfig.enabled = enable;
        pins.waterFlow = sensorPin;
    }

    void enableRS485(bool enable = true, uint8_t rePin = 16, uint8_t dePin = 17) {
        rs485Config.enabled = enable;
        pins.rs485RE = rePin;
        pins.rs485DE = dePin;
    }

    void enableTempHumidity(bool enable = true, uint8_t dataPin = 18) {
        tempHumidityConfig.enabled = enable;
        pins.tempHumidity = dataPin;
    }

    // Set global batch interval
    void setBatchInterval(uint32_t intervalMs) {
        sensorBatchIntervalMs = intervalMs;
        ultrasonicConfig.batchIntervalMs = intervalMs;
        waterLevelConfig.batchIntervalMs = intervalMs;
        waterFlowConfig.batchIntervalMs = intervalMs;
        rs485Config.batchIntervalMs = intervalMs;
        tempHumidityConfig.batchIntervalMs = intervalMs;
    }
};

// Factory function for creating remote sensor config
inline RemoteSensorConfig createRemoteSensorConfig() {
    return RemoteSensorConfig();
}
