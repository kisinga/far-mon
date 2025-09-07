// WiFi Manager - Actual WiFi implementation for Heltec WiFi LoRa 32 V3
// Provides real WiFi connectivity and status monitoring

#pragma once

#include <WiFi.h>

class WifiManager {
public:
    struct Config {
        const char* ssid = nullptr;
        const char* password = nullptr;
        uint32_t reconnectIntervalMs = 30000; // 30 seconds
        uint32_t statusCheckIntervalMs = 5000; // 5 seconds
    };

    explicit WifiManager(const Config& config) : cfg(config) {}

    void begin() {
        if (!cfg.ssid || !cfg.password) {
            Serial.println(F("[WiFi] No SSID/password configured"));
            return;
        }

        Serial.printf("[WiFi] Connecting to %s...\n", cfg.ssid);
        WiFi.begin(cfg.ssid, cfg.password);
        lastReconnectAttempt = millis();
    }

    void update(uint32_t nowMs) {
        // Periodic reconnection attempts if disconnected
        if (!isConnected() && (nowMs - lastReconnectAttempt >= cfg.reconnectIntervalMs)) {
            Serial.println(F("[WiFi] Reconnecting..."));
            WiFi.reconnect();
            lastReconnectAttempt = nowMs;
        }

        // Update cached status periodically
        if (nowMs - lastStatusCheck >= cfg.statusCheckIntervalMs) {
            lastStatusCheck = nowMs;
            updateCachedStatus();
        }
    }

    bool isConnected() const {
        return WiFi.status() == WL_CONNECTED;
    }

    // Get signal strength as percentage (0-100)
    int8_t getSignalStrengthPercent() const {
        if (!isConnected()) return -1;

        int32_t rssi = WiFi.RSSI();
        // Convert RSSI to percentage (typical range: -100 to -30 dBm)
        // RSSI of -30dBm = 100%, RSSI of -100dBm = 0%
        int8_t percent = map(rssi, -100, -30, 0, 100);
        return constrain(percent, 0, 100);
    }

    // Get raw RSSI value
    int32_t getRSSI() const {
        return isConnected() ? WiFi.RSSI() : 0;
    }

    // Get connection info for debugging
    void printStatus() const {
        Serial.printf("[WiFi] Status: %s, RSSI: %ddBm (%d%%), IP: %s\n",
                     isConnected() ? "Connected" : "Disconnected",
                     getRSSI(),
                     getSignalStrengthPercent(),
                     isConnected() ? WiFi.localIP().toString().c_str() : "N/A");
    }

private:
    const Config& cfg;
    uint32_t lastReconnectAttempt = 0;
    uint32_t lastStatusCheck = 0;

    void updateCachedStatus() {
        // Cache expensive operations if needed
        // For now, WiFi.status() is fast enough to call directly
    }
};
