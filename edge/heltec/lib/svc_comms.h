#pragma once

#include "common_message_types.h"
#include "hal_lora.h"
#include "hal_wifi.h"
#include <memory>
#include <vector>

class ICommsService {
public:
    virtual ~ICommsService() = default;

    virtual void setLoraHal(ILoRaHal* loraHal) = 0;
    virtual void setWifiHal(IWifiHal* wifiHal) = 0;
    
    virtual void update(uint32_t nowMs) = 0;
    virtual bool sendMessage(const Messaging::Message& message, TransportType transport) = 0;
};

class CommsService : public ICommsService {
public:
    CommsService();

    void setLoraHal(ILoRaHal* loraHal) override;
    void setWifiHal(IWifiHal* wifiHal) override;
    
    void update(uint32_t nowMs) override;
    bool sendMessage(const Messaging::Message& message, TransportType transport) override;

private:
    ILoRaHal* _loraHal = nullptr;
    IWifiHal* _wifiHal = nullptr;
    // Routing logic will be added here later
};
