// Display Provider Interface - SOLID/KISS/DRY implementation
// Provides a clean abstraction for different display modes

#pragma once

#include <memory>
#include "display.h"

// Forward declarations
class LoRaComm;

// Base interface for header right display providers
class HeaderRightProvider {
public:
    virtual ~HeaderRightProvider() = default;

    // Called by display system to update this provider's state
    virtual void update() = 0;

    // Called by display system to get the current mode
    virtual HeaderRightMode getMode() const = 0;

    // Called by display system to apply this provider's data to the display
    virtual void applyToDisplay(OledDisplay& display) const = 0;
};

// WiFi status provider implementation
class WifiStatusProvider : public HeaderRightProvider {
public:
    explicit WifiStatusProvider(WifiManager& wifiMgr) : wifiManager(wifiMgr) {}

    void update() override {
        wifiConnected = wifiManager.isConnected();
        wifiSignalStrength = wifiManager.getSignalStrengthPercent();
    }

    HeaderRightMode getMode() const override {
        return HeaderRightMode::WifiStatus;
    }

    void applyToDisplay(OledDisplay& display) const override {
        display.setWifiStatus(wifiConnected, wifiSignalStrength);
    }

private:
    WifiManager& wifiManager;
    bool wifiConnected = false;
    int8_t wifiSignalStrength = -1;
};

// LoRa signal provider (for remote devices)
class LoRaSignalProvider : public HeaderRightProvider {
public:
    explicit LoRaSignalProvider(const LoRaComm& loraComm) : lora(loraComm) {}

    void update() override {
        // Update LoRa status from the communication layer
        connected = lora.isConnected();
        rssi = lora.getLastRssiDbm();
    }

    HeaderRightMode getMode() const override {
        return HeaderRightMode::SignalBars;
    }

    void applyToDisplay(OledDisplay& display) const override {
        display.setLoraStatus(connected, rssi);
    }

private:
    const LoRaComm& lora;
    bool connected = false;
    int16_t rssi = -127;
};

// Peer count provider (for relay devices)
class PeerCountProvider : public HeaderRightProvider {
public:
    explicit PeerCountProvider(const LoRaComm& loraComm) : lora(loraComm) {}

    void update() override {
        // Count connected peers
        size_t connectedCount = 0;
        for (size_t i = 0;; i++) {
            LoRaComm::PeerInfo p{};
            if (!lora.getPeerByIndex(i, p)) break;
            if (p.connected) connectedCount++;
        }
        peerCount = static_cast<uint16_t>(connectedCount);
    }

    HeaderRightMode getMode() const override {
        return HeaderRightMode::PeerCount;
    }

    void applyToDisplay(OledDisplay& display) const override {
        display.setPeerCount(peerCount);
    }

private:
    const LoRaComm& lora;
    uint16_t peerCount = 0;
};

// Display Manager - coordinates all display providers
class DisplayManager {
public:
    explicit DisplayManager(OledDisplay& display) : oledDisplay(display) {}

    // Register a header right provider
    void setHeaderRightProvider(std::unique_ptr<HeaderRightProvider> provider) {
        headerRightProvider = std::move(provider);
    }

    // Update all registered providers and apply to display
    void updateAndRefresh() {
        if (headerRightProvider) {
            headerRightProvider->update();
            oledDisplay.setHeaderRightMode(headerRightProvider->getMode());
            headerRightProvider->applyToDisplay(oledDisplay);
        }
        oledDisplay.tick(millis());
    }

private:
    OledDisplay& oledDisplay;
    std::unique_ptr<HeaderRightProvider> headerRightProvider;
};
