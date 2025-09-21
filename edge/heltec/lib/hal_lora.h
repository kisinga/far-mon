#pragma once

#include <stdint.h>
#include "lora_comm.h"

class ILoRaHal {
public:
    enum class Mode : uint8_t { Master = 0, Slave = 1 };

    using OnDataReceived = void (*)(uint8_t srcId, const uint8_t *payload, uint8_t length);
    using OnAckReceived = void (*)(uint8_t srcId, uint16_t messageId);

    virtual ~ILoRaHal() = default;

    virtual bool begin(Mode mode, uint8_t selfId) = 0;
    virtual void tick(uint32_t nowMs) = 0;

    virtual bool sendData(uint8_t destId, const uint8_t *payload, uint8_t length, bool requireAck = true) = 0;
    
    virtual void setOnDataReceived(OnDataReceived cb) = 0;
    virtual void setOnAckReceived(OnAckReceived cb) = 0;
    virtual void setPeerTimeout(uint32_t timeoutMs) = 0;

    virtual bool isConnected() const = 0;
    virtual int16_t getLastRssiDbm() const = 0;
    virtual size_t getPeerCount() const = 0;
    virtual size_t getTotalPeerCount() const = 0;

    // Connection state management
    virtual void setMasterNodeId(uint8_t masterId) = 0;
    virtual void forceReconnect() = 0;
    using ConnectionState = LoRaComm::ConnectionState;
    virtual ConnectionState getConnectionState() const = 0;
};

class LoRaCommHal : public ILoRaHal {
public:
    LoRaCommHal();
    bool begin(Mode mode, uint8_t deviceId) override;
    void tick(uint32_t nowMs) override;
    bool sendData(uint8_t targetId, const uint8_t* data, uint8_t len, bool ack) override;
    void setOnDataReceived(OnDataReceived cb) override;
    void setOnAckReceived(OnAckReceived cb) override;
    void setPeerTimeout(uint32_t timeoutMs) override;
    bool isConnected() const override;
    int16_t getLastRssiDbm() const override;
    size_t getPeerCount() const override;
    size_t getTotalPeerCount() const override;

    // Connection state management
    void setMasterNodeId(uint8_t masterId) override;
    void forceReconnect() override;
    ConnectionState getConnectionState() const override;

private:
    LoRaComm _lora;
};

LoRaCommHal::LoRaCommHal() : _lora() {}

bool LoRaCommHal::begin(Mode mode, uint8_t deviceId) {
    LoRaComm::Mode loraMode = (mode == Mode::Master) ? LoRaComm::Mode::Master : LoRaComm::Mode::Slave;
    return _lora.safeBegin(loraMode, deviceId);
}

void LoRaCommHal::tick(uint32_t nowMs) {
    _lora.tick(nowMs);
}

bool LoRaCommHal::sendData(uint8_t targetId, const uint8_t* data, uint8_t len, bool ack) {
    return _lora.sendData(targetId, data, len, ack);
}

void LoRaCommHal::setOnDataReceived(OnDataReceived cb) {
    _lora.setOnDataReceived(cb);
}

void LoRaCommHal::setOnAckReceived(OnAckReceived cb) {
    _lora.setOnAckReceived(cb);
}

void LoRaCommHal::setPeerTimeout(uint32_t timeoutMs) {
    _lora.setPeerTimeout(timeoutMs);
}

bool LoRaCommHal::isConnected() const {
    return _lora.isConnected();
}

int16_t LoRaCommHal::getLastRssiDbm() const {
    return _lora.getLastRssiDbm();
}

size_t LoRaCommHal::getPeerCount() const {
    return _lora.getPeerCount();
}

size_t LoRaCommHal::getTotalPeerCount() const {
    return _lora.getTotalPeerCount();
}

// Connection state management
void LoRaCommHal::setMasterNodeId(uint8_t masterId) {
    _lora.setMasterNodeId(masterId);
}

void LoRaCommHal::forceReconnect() {
    _lora.forceReconnect();
}

LoRaCommHal::ConnectionState LoRaCommHal::getConnectionState() const {
    return _lora.getConnectionState();
}
