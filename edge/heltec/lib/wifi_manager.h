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

    explicit WifiManager(const Config& config) : cfg(config), initialized(false) {}

    // Safe begin that prevents double initialization
    // Returns true if initialization was performed, false if already initialized
    bool safeBegin() {
        if (initialized) {
            return false; // Already initialized
        }
        unsafeBegin();
        return true;
    }
    
    // Public update API used by services/transports
    void update(uint32_t nowMs) {
        // Periodic reconnection attempts with backoff if disconnected
        if (!isConnected()) {
            // Initialize backoff window on first use
            if (currentReconnectBackoffMs == 0) {
                currentReconnectBackoffMs = cfg.reconnectIntervalMs > 0 ? cfg.reconnectIntervalMs : 1000;
                if (currentReconnectBackoffMs > kMaxReconnectBackoffMs) currentReconnectBackoffMs = kMaxReconnectBackoffMs;
            }

            if (nowMs - lastReconnectAttempt >= currentReconnectBackoffMs) {
                Serial.println(F("[WiFi] Reconnecting..."));
                WiFi.reconnect();
                lastReconnectAttempt = nowMs;

                // Exponential backoff capped at 15s
                uint32_t next = currentReconnectBackoffMs * 2;
                currentReconnectBackoffMs = next > kMaxReconnectBackoffMs ? kMaxReconnectBackoffMs : next;
            }
        } else {
            // Reset backoff once connected
            currentReconnectBackoffMs = cfg.reconnectIntervalMs > 0 ? cfg.reconnectIntervalMs : 1000;
            if (currentReconnectBackoffMs > kMaxReconnectBackoffMs) currentReconnectBackoffMs = kMaxReconnectBackoffMs;
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

    // Uplink helper: placeholder for HTTP/MQTT/etc. Currently logs payload.
    // Returns true if accepted for send.
    bool uplink(const uint8_t* payload, uint8_t length) {
        if (!payload || length == 0) return false;
        if (!isConnected()) return false;
        Serial.print(F("[WiFi] Uplink: "));
        for (uint8_t i = 0; i < length; i++) Serial.write(payload[i]);
        Serial.println();
        return true;
    }

private:
    // Internal unsafe begin - should not be called directly
    void unsafeBegin() {
        if (!cfg.ssid || !cfg.password) {
            Serial.println(F("[WiFi] No SSID/password configured"));
            return;
        }

        Serial.printf("[WiFi] Connecting to %s...\n", cfg.ssid);
        WiFi.begin(cfg.ssid, cfg.password);
        lastReconnectAttempt = millis();
        currentReconnectBackoffMs = cfg.reconnectIntervalMs > 0 ? cfg.reconnectIntervalMs : 1000;
        if (currentReconnectBackoffMs > kMaxReconnectBackoffMs) currentReconnectBackoffMs = kMaxReconnectBackoffMs;

        initialized = true;
    }

private:
    const Config& cfg;
    uint32_t lastReconnectAttempt = 0;
    uint32_t lastStatusCheck = 0;
    bool initialized;
    uint32_t currentReconnectBackoffMs = 0; // dynamic backoff interval (ms)
    static constexpr uint32_t kMaxReconnectBackoffMs = 15000; // 15 seconds cap

    void updateCachedStatus() {
        // Cache expensive operations if needed
        // For now, WiFi.status() is fast enough to call directly
    }
};
