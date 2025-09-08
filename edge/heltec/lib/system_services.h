// System Services - Centralized service management with dependency injection
// Provides clean interfaces for all system services

#pragma once

#include <memory>
#include <functional>
#include "config.h"
#include "display_provider.h"
#include "wifi_manager.h"
#include "battery_monitor.h"
#include "lora_comm.h"
#include "scheduler.h"

// Forward declarations
class OledDisplay;

// Service interfaces
class IBatteryService {
public:
    virtual ~IBatteryService() = default;
    virtual void update(uint32_t nowMs) = 0;
    virtual uint8_t getBatteryPercent() const = 0;
    virtual bool isCharging() const = 0;
};

class IDisplayService {
public:
    virtual ~IDisplayService() = default;
    virtual void update(uint32_t nowMs) = 0;
    virtual void setHeaderProvider(std::unique_ptr<HeaderRightProvider> provider) = 0;
    virtual void getContentArea(int16_t& x, int16_t& y, int16_t& w, int16_t& h) const = 0;
    virtual void setBatteryStatus(bool valid, uint8_t percent) = 0;
    virtual void setBatteryCharging(bool charging) = 0;
    virtual void tick(uint32_t nowMs) = 0;
};

class IWifiService {
public:
    virtual ~IWifiService() = default;
    virtual void update(uint32_t nowMs) = 0;
    virtual bool isConnected() const = 0;
    virtual int8_t getSignalStrengthPercent() const = 0;
};

class ILoRaService {
public:
    virtual ~ILoRaService() = default;
    virtual void update(uint32_t nowMs) = 0;
    virtual bool isConnected() const = 0;
    virtual int16_t getLastRssiDbm() const = 0;
    virtual void sendData(uint8_t dest, const uint8_t* payload, uint8_t len, bool requireAck = false) = 0;
    virtual size_t getPeerCount() const = 0;
};

// Concrete service implementations
class BatteryService : public IBatteryService {
public:
    explicit BatteryService(BatteryMonitor::BatteryMonitor& monitor) : batteryMonitor(monitor) {}

    void update(uint32_t nowMs) override {
        batteryMonitor.updateChargeStatus(nowMs);
    }

    uint8_t getBatteryPercent() const override {
        uint8_t percent = 255;
        batteryMonitor.readPercent(percent);
        return percent;
    }

    bool isCharging() const override {
        return batteryMonitor.isCharging();
    }

private:
    BatteryMonitor::BatteryMonitor& batteryMonitor;
};

class DisplayService : public IDisplayService {
public:
    explicit DisplayService(OledDisplay& display) : oledDisplay(display) {}

    void update(uint32_t nowMs) override {
        // Display update is handled by tick() method
    }

    void setHeaderProvider(std::unique_ptr<HeaderRightProvider> provider) override {
        headerProvider = std::move(provider);
    }

    void getContentArea(int16_t& x, int16_t& y, int16_t& w, int16_t& h) const override {
        oledDisplay.getContentArea(x, y, w, h);
    }

    void setBatteryStatus(bool valid, uint8_t percent) override {
        oledDisplay.setBatteryStatus(valid, percent);
    }

    void setBatteryCharging(bool charging) override {
        oledDisplay.setBatteryCharging(charging);
    }

    void tick(uint32_t nowMs) override {
        oledDisplay.tick(nowMs);
    }

private:
    OledDisplay& oledDisplay;
    std::unique_ptr<HeaderRightProvider> headerProvider;
};

class WifiService : public IWifiService {
public:
    explicit WifiService(WifiManager& manager) : wifiManager(manager) {}

    void update(uint32_t nowMs) override {
        wifiManager.update(nowMs);
    }

    bool isConnected() const override {
        return wifiManager.isConnected();
    }

    int8_t getSignalStrengthPercent() const override {
        return wifiManager.getSignalStrengthPercent();
    }

private:
    WifiManager& wifiManager;
};

class LoRaService : public ILoRaService {
public:
    explicit LoRaService(LoRaComm& comm) : loraComm(comm) {}

    void update(uint32_t nowMs) override {
        loraComm.tick(nowMs);
        Radio.IrqProcess();
    }

    bool isConnected() const override {
        return loraComm.isConnected();
    }

    int16_t getLastRssiDbm() const override {
        return loraComm.getLastRssiDbm();
    }

    void sendData(uint8_t dest, const uint8_t* payload, uint8_t len, bool requireAck = false) override {
        loraComm.sendData(dest, payload, len, requireAck);
    }

    size_t getPeerCount() const override {
        size_t count = 0;
        for (size_t i = 0; ; i++) {
            LoRaComm::PeerInfo p{};
            if (!loraComm.getPeerByIndex(i, p)) break;
            if (p.connected) count++;
        }
        return count;
    }

private:
    LoRaComm& loraComm;
};

// Service container with dependency injection
struct SystemServices {
    std::unique_ptr<IBatteryService> battery;
    std::unique_ptr<IDisplayService> display;
    std::unique_ptr<IWifiService> wifi;
    std::unique_ptr<ILoRaService> lora;

    // References to concrete objects for tasks that need them
    OledDisplay* oledDisplay = nullptr;

    // Factory method for creating services
    static SystemServices create(OledDisplay& oledDisplay,
                                WifiManager& wifiManager,
                                BatteryMonitor::BatteryMonitor& batteryMonitor,
                                LoRaComm& loraComm) {
        SystemServices services;

        services.battery = std::make_unique<BatteryService>(batteryMonitor);
        services.display = std::make_unique<DisplayService>(oledDisplay);
        services.wifi = std::make_unique<WifiService>(wifiManager);
        services.lora = std::make_unique<LoRaService>(loraComm);

        // Store references for tasks that need direct access
        services.oledDisplay = &oledDisplay;

        return services;
    }
};
