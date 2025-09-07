// Unified WiFi Configuration System - DRY Implementation
// Provides centralized WiFi configuration for all devices

#pragma once

#include "wifi_manager.h"

// ============================================================================
// UNIFIED WIFI CONFIGURATION - DRY Implementation
// ============================================================================

class WiFiConfigFactory {
public:
    // Common configuration defaults
    static constexpr uint32_t DEFAULT_RECONNECT_INTERVAL_MS = 30000;
    static constexpr uint32_t DEFAULT_STATUS_CHECK_INTERVAL_MS = 5000;

    // Create WiFi config for relay device
    static WifiManager::Config createRelayConfig() {
        return createBaseConfig("STARLINK", "awesome33");
    }

    // Create WiFi config for remote device
    static WifiManager::Config createRemoteConfig() {
        return createBaseConfig("YourWiFiNetwork", "YourWiFiPassword");
    }

    // Create custom WiFi config
    static WifiManager::Config createCustomConfig(const char* ssid, const char* password,
                                                  uint32_t reconnectIntervalMs = DEFAULT_RECONNECT_INTERVAL_MS,
                                                  uint32_t statusCheckIntervalMs = DEFAULT_STATUS_CHECK_INTERVAL_MS) {
        return createBaseConfig(ssid, password, reconnectIntervalMs, statusCheckIntervalMs);
    }

private:
    static WifiManager::Config createBaseConfig(const char* ssid, const char* password,
                                                uint32_t reconnectIntervalMs = DEFAULT_RECONNECT_INTERVAL_MS,
                                                uint32_t statusCheckIntervalMs = DEFAULT_STATUS_CHECK_INTERVAL_MS) {
        WifiManager::Config config;
        config.ssid = ssid;
        config.password = password;
        config.reconnectIntervalMs = reconnectIntervalMs;
        config.statusCheckIntervalMs = statusCheckIntervalMs;
        return config;
    }
};

// (Legacy compatibility shims removed; use WiFiConfigFactory or per-device CommunicationConfig.)
