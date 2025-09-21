#include "svc_lora.h"

LoRaService::LoRaService(ILoRaHal& hal) : loraHal(hal) {}

void LoRaService::update(uint32_t nowMs) {
    loraHal.tick(nowMs);
}

bool LoRaService::isConnected() const {
    return loraHal.isConnected();
}

int16_t LoRaService::getLastRssiDbm() const {
    return loraHal.getLastRssiDbm();
}

void LoRaService::sendData(uint8_t dest, const uint8_t* payload, uint8_t len, bool requireAck) {
    loraHal.sendData(dest, payload, len, requireAck);
}

size_t LoRaService::getPeerCount() const {
    return loraHal.getPeerCount();
}
