#pragma once

#include <stdint.h>
#include "wifi_manager.h"

class IWifiHal {
public:
    virtual ~IWifiHal() = default;

    virtual bool begin() = 0;
    virtual void update(uint32_t nowMs) = 0;
    
    virtual bool isConnected() const = 0;
    virtual int8_t getSignalStrengthPercent() const = 0;
    virtual int32_t getRSSI() const = 0;
    
    virtual bool uplink(const uint8_t* payload, uint8_t length) = 0;
};

class WifiManagerHal : public IWifiHal {
public:
    explicit WifiManagerHal(const WifiManager::Config& config);
    bool begin() override;
    void update(uint32_t nowMs) override;
    bool isConnected() const override;
    int8_t getSignalStrengthPercent() const override;
    int32_t getRSSI() const override;
    bool uplink(const uint8_t* payload, uint8_t length) override;

private:
    WifiManager _wifiManager;
};

WifiManagerHal::WifiManagerHal(const WifiManager::Config& config) : _wifiManager(config) {}

bool WifiManagerHal::begin() {
    return _wifiManager.safeBegin();
}

void WifiManagerHal::update(uint32_t nowMs) {
    _wifiManager.update(nowMs);
}

bool WifiManagerHal::isConnected() const {
    return _wifiManager.isConnected();
}

int8_t WifiManagerHal::getSignalStrengthPercent() const {
    return _wifiManager.getSignalStrengthPercent();
}

int32_t WifiManagerHal::getRSSI() const {
    return _wifiManager.getRSSI();
}

bool WifiManagerHal::uplink(const uint8_t* payload, uint8_t length) {
    return _wifiManager.uplink(payload, length);
}
