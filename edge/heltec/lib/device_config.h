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

// Device-specific configs are declared in:
// - relay/relay_config.h
// - remote/remote_config.h
