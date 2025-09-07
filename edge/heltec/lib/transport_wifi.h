// TransportWiFi - WiFi transport implementation
// Wraps WifiManager to provide Message-based communication

#pragma once

#include "transport_interface.h"
#include "wifi_manager.h"
#include "config.h"

class TransportWiFi : public TransportInterface {
public:
    TransportWiFi(uint8_t id, const WifiCommConfig& wifiConfig)
        : TransportInterface(TransportType::WiFi, id) {
        // Convert communication config to WifiManager config
        WifiManager::Config wmConfig;
        wmConfig.ssid = wifiConfig.ssid;
        wmConfig.password = wifiConfig.password;
        wmConfig.reconnectIntervalMs = wifiConfig.reconnectIntervalMs;
        wmConfig.statusCheckIntervalMs = wifiConfig.statusCheckIntervalMs;
        wifiManager = std::make_unique<WifiManager>(wmConfig);
    }

    ~TransportWiFi() override {
        end();
    }

    // TransportInterface implementation
    bool begin() override {
        if (wifiManager) {
            wifiManager->begin();
            onConnectionStateChanged(ConnectionState::Connecting);
            return true;
        }
        return false;
    }

    void update(uint32_t nowMs) override {
        if (wifiManager) {
            wifiManager->update(nowMs);

            // Update connection state
            ConnectionState newState = wifiManager->isConnected() ?
                                      ConnectionState::Connected : ConnectionState::Disconnected;
            if (newState != getConnectionState()) {
                onConnectionStateChanged(newState);
            }
        }
    }

    void end() override {
        // WiFi doesn't have explicit shutdown, just mark as disconnected
        onConnectionStateChanged(ConnectionState::Disconnected);
    }

    bool sendMessage(const Message& message) override {
        if (!canSendMessage() || !wifiManager) return false;

        // Use the uplink method we added to WifiManager
        return wifiManager->uplink(message.getPayload(), message.getLength());
    }

    bool canSendMessage() const override {
        return wifiManager && wifiManager->isConnected();
    }

    TransportCapabilities getCapabilities() const override {
        return TransportCapabilities{
            .canSend = true,
            .canReceive = false, // WiFi is primarily outbound in this design
            .supportsAck = false, // Current uplink doesn't support ACK
            .supportsBroadcast = false,
            .requiresConnection = true,
            .isReliable = false // Depends on underlying protocol
        };
    }

    const char* getName() const override { return "WiFi"; }

    // WiFi-specific methods
    WifiManager* getWifiManager() { return wifiManager.get(); }
    const WifiManager* getWifiManager() const { return wifiManager.get(); }

private:
    std::unique_ptr<WifiManager> wifiManager;
};
