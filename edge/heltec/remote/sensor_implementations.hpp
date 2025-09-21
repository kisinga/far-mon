#ifndef SENSOR_IMPLEMENTATIONS_HPP
#define SENSOR_IMPLEMENTATIONS_HPP

#include <Arduino.h>
#include "sensor_interface.hpp"
#include "lib/core_logger.h"
#include "lib/hal_lora.h"
#include <vector>

// ============================================================================
// LoRa Batch Transmitter Implementation
// ============================================================================

class LoRaBatchTransmitter : public SensorBatchTransmitter {
public:
    LoRaBatchTransmitter(ILoRaHal* loraHal, uint8_t deviceId);
    bool transmitBatch(const std::vector<SensorReading>& readings) override;
    bool isReady() const override;

private:
    String formatReadings(const std::vector<SensorReading>& readings);
    ILoRaHal* _loraHal;
    uint8_t _deviceId;
};

LoRaBatchTransmitter::LoRaBatchTransmitter(ILoRaHal* loraHal, uint8_t deviceId)
    : _loraHal(loraHal), _deviceId(deviceId) {
}

bool LoRaBatchTransmitter::transmitBatch(const std::vector<SensorReading>& readings) {
    if (!_loraHal || readings.empty()) return false;

    String payload = formatReadings(readings);

    if (payload.length() == 0) return false;

    // Send as LoRa message (no ACK for telemetry)
    return _loraHal->sendData(1, (const uint8_t*)payload.c_str(),
                             (uint8_t)payload.length(), false);
}

bool LoRaBatchTransmitter::isReady() const {
    return _loraHal && _loraHal->isConnected();
}

String LoRaBatchTransmitter::formatReadings(const std::vector<SensorReading>& readings) {
    // This is a placeholder for a more robust serialization format like JSON or MessagePack
    String payload = "";
    for (size_t i = 0; i < readings.size(); ++i) {
        payload += readings[i].type; // This now works because type is const char*
        payload += ":";
        payload += String(readings[i].value, 2);
        if (i < readings.size() - 1) {
            payload += ",";
        }
    }
    return payload;
}

// ============================================================================
// SENSOR FACTORY (Placeholder - not used for now)
// ============================================================================

namespace SensorFactory {
    // In a real implementation, you would have factory functions here like:
    // std::unique_ptr<ISensor> createUltrasonicSensor(...);
}

#endif // SENSOR_IMPLEMENTATIONS_HPP
