// Device Configuration System - Unified configuration management
// Provides centralized, DRY configuration for all device types

#pragma once

#include <cstdint>
#include <cstring>
#include "communication_config.h"

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
    uint32_t displayUpdateIntervalMs;

    // Centralized communication configuration
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
    static DeviceConfig createRelayConfig(const char* deviceId) {
        DeviceConfig config = createBaseConfig(deviceId, DeviceType::Relay);

        // Relay-specific communication settings
        config.communication.usb.enableDebug = true;
        config.communication.usb.baudRate = 115200;

        config.communication.lora.enableLora = true;
        config.communication.lora.frequency = 868000000UL;
        config.communication.lora.txPower = 14;

        config.communication.wifi.enableWifi = true;
        config.communication.wifi.ssid = "STARLINK";
        config.communication.wifi.password = "awesome33";

        config.communication.screen.enableScreen = true;

        // Set up routing rules for relay: LoRa -> WiFi, LoRa -> USB, Telemetry -> Screen
        config.communication.routing.enableRouting = true;
        config.communication.routing.routingIntervalMs = DEFAULT_ROUTING_INTERVAL_MS;

        // Add default routes (by transport type)
        config.communication.routing.routes[0] = {Message::Type::Data, TransportType::LoRa, TransportType::WiFi, true, 0};        // LoRa -> WiFi
        config.communication.routing.routes[1] = {Message::Type::Data, TransportType::LoRa, TransportType::USB_Debug, true, 1};    // LoRa -> USB
        config.communication.routing.routes[2] = {Message::Type::Telemetry, TransportType::LoRa, TransportType::Screen, true, 0};  // LoRa -> Screen
        config.communication.routing.routeCount = 3;

        config.displayUpdateIntervalMs = 800;
        return config;
    }

    // Create remote configuration
    static DeviceConfig createRemoteConfig(const char* deviceId) {
        DeviceConfig config = createBaseConfig(deviceId, DeviceType::Remote);

        // Remote-specific communication settings
        config.communication.usb.enableDebug = true;
        config.communication.usb.baudRate = 115200;

        config.communication.wifi.enableWifi = false;  // Remotes typically don't need WiFi

        config.communication.lora.enableLora = true;
        config.communication.lora.frequency = 868000000UL;
        config.communication.lora.txPower = 14;

        config.communication.screen.enableScreen = true;

        // Set up routing rules for remote: Telemetry -> LoRa
        config.communication.routing.enableRouting = true;
        config.communication.routing.routingIntervalMs = DEFAULT_ROUTING_INTERVAL_MS;

        // Add default routes (by transport type)
        config.communication.routing.routes[0] = {Message::Type::Telemetry, TransportType::Unknown, TransportType::LoRa, true, 0}; // Any -> LoRa
        config.communication.routing.routeCount = 1;

        config.displayUpdateIntervalMs = 200;
        return config;
    }

private:
    // Create base configuration common to all devices
    static DeviceConfig createBaseConfig(const char* deviceId, DeviceType deviceType) {
        DeviceConfig config;

        // Set device identity
        config.deviceId = deviceId;
        config.deviceType = deviceType;

        // Set common defaults
        config.heartbeatIntervalMs = DEFAULT_HEARTBEAT_INTERVAL_MS;
        config.enableDisplay = true;
        config.displayUpdateIntervalMs = DEFAULT_DISPLAY_UPDATE_INTERVAL_MS;

        // Initialize communication configuration with defaults
        config.communication = CommunicationConfig::createDefault();

        // Apply common communication settings
        config.communication.enableCommunicationManager = true;
        config.communication.updateIntervalMs = DEFAULT_ROUTING_INTERVAL_MS;
        config.communication.maxConcurrentMessages = 8;
        config.communication.enableMessageBuffering = true;
        config.communication.bufferSize = 1024;

        // Apply common WiFi settings (can be overridden by device-specific configs)
        config.communication.wifi.reconnectIntervalMs = DEFAULT_WIFI_RECONNECT_INTERVAL_MS;
        config.communication.wifi.statusCheckIntervalMs = DEFAULT_WIFI_STATUS_CHECK_INTERVAL_MS;
        config.communication.wifi.maxReconnectAttempts = 10;
        config.communication.wifi.enableAutoReconnect = true;
        config.communication.wifi.connectionTimeoutMs = 15000;

        // Apply common LoRa settings
        config.communication.lora.maxPayload = 64;
        config.communication.lora.maxOutbox = 8;
        config.communication.lora.maxPeers = 16;
        config.communication.lora.ackTimeoutMs = 1500;
        config.communication.lora.maxRetries = 4;
        config.communication.lora.pingIntervalMs = 30000;
        config.communication.lora.masterTtlMs = 15000;

        // Apply common USB settings
        config.communication.usb.enableTimestamp = true;
        config.communication.usb.rxBufferSize = 256;
        config.communication.usb.txBufferSize = 256;

        // Apply common screen settings
        config.communication.screen.maxLines = 8;
        config.communication.screen.enableAutoScroll = true;
        config.communication.screen.enableTimestamp = true;
        config.communication.screen.messageTimeoutMs = 5000;

        return config;
    }
};

// ============================================================================
// DEVICE-SPECIFIC CONFIG STRUCTURES (for backward compatibility)
// ============================================================================

// Relay configuration - now just a thin wrapper
struct RelayConfig : DeviceConfig {
    uint32_t peerMonitorIntervalMs = 2000;
    uint8_t maxPeers = 16;

    RelayConfig() : DeviceConfig(DeviceConfigFactory::createRelayConfig("")) {
        deviceType = DeviceType::Relay;
    }

    // Factory method
    static RelayConfig create(const char* deviceId) {
        RelayConfig config;
        DeviceConfig base = DeviceConfigFactory::createRelayConfig(deviceId);
        memcpy(&config, &base, sizeof(DeviceConfig));
        config.deviceId = deviceId;  // Ensure deviceId is set correctly
        config.peerMonitorIntervalMs = 2000;
        config.maxPeers = 16;
        return config;
    }
};

// Remote configuration - now just a thin wrapper
struct RemoteConfig : DeviceConfig {
    bool enableAnalogSensor = true;
    uint8_t analogInputPin = 34;  // Default for Heltec LoRa 32 V3
    uint32_t analogReadIntervalMs = 200;
    uint32_t telemetryReportIntervalMs = 2000;
    float analogReferenceVoltage = 3.30f;
    uint8_t masterNodeId = 1;

    RemoteConfig() : DeviceConfig(DeviceConfigFactory::createRemoteConfig("")) {
        deviceType = DeviceType::Remote;
    }

    // Factory method
    static RemoteConfig create(const char* deviceId) {
        RemoteConfig config;
        DeviceConfig base = DeviceConfigFactory::createRemoteConfig(deviceId);
        memcpy(&config, &base, sizeof(DeviceConfig));
        config.deviceId = deviceId;  // Ensure deviceId is set correctly

        // Set remote-specific defaults
        config.enableAnalogSensor = true;
        config.analogInputPin = 34;
        config.analogReadIntervalMs = 200;
        config.telemetryReportIntervalMs = 2000;
        config.analogReferenceVoltage = 3.30f;
        config.masterNodeId = 1;

        return config;
    }
};

// ============================================================================
// LEGACY COMPATIBILITY FUNCTIONS
// ============================================================================

// Factory functions for backward compatibility
inline RelayConfig createRelayConfig(const char* deviceId) {
    return RelayConfig::create(deviceId);
}

inline RemoteConfig createRemoteConfig(const char* deviceId) {
    return RemoteConfig::create(deviceId);
}
