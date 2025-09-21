#pragma once

#include <stdint.h>
#include "hal_lora.h"

class ILoRaService {
public:
    virtual ~ILoRaService() = default;
    virtual void update(uint32_t nowMs) = 0;
    virtual bool isConnected() const = 0;
    virtual int16_t getLastRssiDbm() const = 0;
    virtual void sendData(uint8_t dest, const uint8_t* payload, uint8_t len, bool requireAck = false) = 0;
    virtual size_t getPeerCount() const = 0;
};

class LoRaService : public ILoRaService {
public:
    explicit LoRaService(ILoRaHal& hal);

    void update(uint32_t nowMs) override;
    bool isConnected() const override;
    int16_t getLastRssiDbm() const override;
    void sendData(uint8_t dest, const uint8_t* payload, uint8_t len, bool requireAck = false) override;
    size_t getPeerCount() const override;

private:
    ILoRaHal& loraHal;
};
