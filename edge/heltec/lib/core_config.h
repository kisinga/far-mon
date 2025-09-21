// Core Configuration System - Unified configuration management
// Provides centralized, DRY configuration for all device types

#pragma once

#include <stdint.h>
#include <string.h>
#include "communication_config.h"
#include "battery_monitor.h" // Include battery monitor for its config struct

// Device types
enum class DeviceType : uint8_t {
    Relay = 1,
    Remote = 2
};

// Common device configuration
struct DeviceConfig {
    uint8_t deviceId;
    DeviceType deviceType;
    uint32_t heartbeatIntervalMs;
    bool enableDisplay;
    uint32_t displayUpdateIntervalMs;

    // Centralized hardware and communication configuration
    BatteryMonitor::Config battery;
    CommunicationConfig communication;
};

// ============================================================================
// UNIFIED CONFIGURATION FACTORY - DRY Implementation
// ============================================================================

class DeviceConfigFactory {
public:
    // Common configuration defaults
    static constexpr uint32_t DEFAULT_HEARTBEAT_INTERVAL_MS = 1000;
    static constexpr uint32_t DEFAULT_DISPLAY_UPDATE_INTERVAL_MS = 1000;
    static constexpr uint32_t DEFAULT_ROUTING_INTERVAL_MS = 100;
    static constexpr uint32_t DEFAULT_WIFI_RECONNECT_INTERVAL_MS = 30000;
    static constexpr uint32_t DEFAULT_WIFI_STATUS_CHECK_INTERVAL_MS = 5000;

    // Create relay configuration
    static DeviceConfig createRelayConfig(uint8_t deviceId);

    // Create remote configuration
    static DeviceConfig createRemoteConfig(uint8_t deviceId);

private:
    // Create base configuration common to all devices
    static DeviceConfig createBaseConfig(uint8_t deviceId, DeviceType deviceType);
};

// ============================================================================
// DEVICE-SPECIFIC CONFIG STRUCTURES (for backward compatibility)
// ============================================================================

// Relay configuration - now just a thin wrapper
struct RelayConfig : DeviceConfig {
    uint32_t peerMonitorIntervalMs = 2000;
    uint8_t maxPeers = 16;

    RelayConfig() = default;

    // Factory method
    static RelayConfig create(uint8_t deviceId);
};

// Remote configuration - now just a thin wrapper
struct RemoteConfig : DeviceConfig {
    // Legacy analog sensor support (for backward compatibility)
    bool enableAnalogSensor = true;
    uint8_t analogInputPin = 34;  // Default for Heltec LoRa 32 V3
    uint32_t analogReadIntervalMs = 200;
    uint32_t telemetryReportIntervalMs = 2000;
    float analogReferenceVoltage = 3.30f;
    uint8_t masterNodeId = 1;
    bool useCalibratedAdc = true;  // Use analogReadMilliVolts() for better accuracy

    RemoteConfig() = default;

    // Factory method
    static RemoteConfig create(uint8_t deviceId);
};

// ============================================================================
// LEGACY COMPATIBILITY FUNCTIONS
// ============================================================================

// Factory functions for backward compatibility
inline RelayConfig createRelayConfig(uint8_t deviceId) {
    return RelayConfig::create(deviceId);
}

inline RemoteConfig createRemoteConfig(uint8_t deviceId) {
    return RemoteConfig::create(deviceId);
}
