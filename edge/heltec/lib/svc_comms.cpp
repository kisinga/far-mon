#include "svc_comms.h"

CommsService::CommsService() {}

void CommsService::setLoraHal(ILoRaHal* loraHal) {
    _loraHal = loraHal;
}

void CommsService::setWifiHal(IWifiHal* wifiHal) {
    _wifiHal = wifiHal;
}

void CommsService::update(uint32_t nowMs) {
    if (_loraHal) {
        _loraHal->tick(nowMs);
    }
    if (_wifiHal) {
        _wifiHal->update(nowMs);
    }
}

bool CommsService::sendMessage(const Messaging::Message& message, TransportType transport) {
    switch (transport) {
        case TransportType::LoRa:
            if (_loraHal) {
                return _loraHal->sendData(message.getMetadata().destinationId, message.getPayload(), message.getLength(), message.getMetadata().requiresAck);
            }
            break;
        case TransportType::WiFi:
            if (_wifiHal) {
                return _wifiHal->uplink(message.getPayload(), message.getLength());
            }
            break;
        // Other transport types can be added here
        default:
            return false;
    }
    return false;
}
