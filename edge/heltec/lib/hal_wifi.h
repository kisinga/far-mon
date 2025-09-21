#pragma once

#include <stdint.h>
#include "wifi_manager.h"
#include "mqtt_publisher.h"

class IWifiHal {
public:
    virtual ~IWifiHal() = default;

    virtual bool begin() = 0;
    virtual void update(uint32_t nowMs) = 0;

    virtual bool isConnected() const = 0;
    virtual int8_t getSignalStrengthPercent() const = 0;
    virtual int32_t getRSSI() const = 0;

    virtual bool uplink(const uint8_t* payload, uint8_t length) = 0;

    // MQTT support
    virtual void setMqttConfig(const MqttPublisherConfig& config) = 0;
    virtual bool publishMqtt(const char* topicSuffix, const uint8_t* payload, uint8_t length) = 0;
    virtual bool isMqttReady() const = 0;
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

    // MQTT support
    void setMqttConfig(const MqttPublisherConfig& config) override;
    bool publishMqtt(const char* topicSuffix, const uint8_t* payload, uint8_t length) override;
    bool isMqttReady() const override;

private:
    WifiManager _wifiManager;
    std::unique_ptr<MqttPublisher> _mqttPublisher;
};

WifiManagerHal::WifiManagerHal(const WifiManager::Config& config) : _wifiManager(config) {}

bool WifiManagerHal::begin() {
    return _wifiManager.safeBegin();
}

void WifiManagerHal::update(uint32_t nowMs) {
    _wifiManager.update(nowMs);
    if (_mqttPublisher) {
        _mqttPublisher->update(nowMs);
    }
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

// MQTT support
void WifiManagerHal::setMqttConfig(const MqttPublisherConfig& config) {
    _mqttPublisher = std::make_unique<MqttPublisher>(config);
}

bool WifiManagerHal::publishMqtt(const char* topicSuffix, const uint8_t* payload, uint8_t length) {
    if (_mqttPublisher) {
        return _mqttPublisher->publish(topicSuffix, payload, length);
    }
    return false;
}

bool WifiManagerHal::isMqttReady() const {
    return _mqttPublisher && _mqttPublisher->isReady();
}
