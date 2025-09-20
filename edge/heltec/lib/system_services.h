// System Services - Centralized service management with dependency injection
// Provides clean interfaces for all system services

#pragma once

#include <memory>
#include <functional>
#include "config.h"
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
    std::unique_ptr<IWifiService> wifi;
    std::unique_ptr<ILoRaService> lora;

    // References to concrete objects for tasks that need them
    OledDisplay* oledDisplay = nullptr;
    LoRaComm* loraComm = nullptr;


    // Factory method for creating services
    static SystemServices create(OledDisplay& oled,
                                WifiManager& wifiManager,
                                BatteryMonitor::BatteryMonitor& batteryMonitor,
                                LoRaComm& lora) {
        SystemServices services;

        services.battery = std::make_unique<BatteryService>(batteryMonitor);
        services.wifi = std::make_unique<WifiService>(wifiManager);
        services.lora = std::make_unique<LoRaService>(lora);

        // Store references for tasks that need direct access
        services.oledDisplay = &oled;
        services.loraComm = &lora;


        return services;
    }
};
