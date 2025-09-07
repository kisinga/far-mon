// Device Configuration System - Device-specific configuration management
// Provides centralized configuration for different device types

#pragma once

#include <cstdint>

// Device types
enum class DeviceType : uint8_t {
    Relay = 1,
    Remote = 2
};

// Common device configuration
struct DeviceConfig {
    const char* deviceId;
    DeviceType deviceType;
    uint32_t heartbeatIntervalMs;
    bool enableDisplay;
    bool enableDebug;
    uint32_t displayUpdateIntervalMs;
    uint32_t loraTaskIntervalMs;
};

// Relay-specific configuration
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

// Remote-specific configuration
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

// Configuration factory functions
RelayConfig createRelayConfig(const char* deviceId);
RemoteConfig createRemoteConfig(const char* deviceId);
