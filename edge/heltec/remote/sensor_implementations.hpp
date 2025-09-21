#ifndef SENSOR_IMPLEMENTATIONS_HPP
#define SENSOR_IMPLEMENTATIONS_HPP

#include <Arduino.h>
#include "sensor_interface.hpp"
#include "lib/core_logger.h"
#include "lib/hal_lora.h"
#include "lib/common_message_types.h"
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
    if (!_loraHal || readings.empty()) {
        LOGD("LoRaBatchTransmitter", "Cannot transmit: %s",
             !_loraHal ? "LoRa HAL not available" : "no sensor readings");
        return false;
    }

    String payload = formatReadings(readings);
    LOGD("LoRaBatchTransmitter", "Formatted %u sensor readings into payload: '%s'",
         (unsigned)readings.size(), payload.c_str());

    if (payload.length() == 0) {
        LOGW("LoRaBatchTransmitter", "Failed to format sensor readings for transmission");
        return false;
    }

    // Create a telemetry message
    Messaging::Message message(
        Messaging::Message::Type::Telemetry,
        _deviceId,  // source ID
        1,          // destination ID (relay)
        true,       // ACK required for telemetry
        (const uint8_t*)payload.c_str(),
        (uint16_t)payload.length()
    );

    // Send as LoRa message using the proper message format
    LOGD("LoRaBatchTransmitter", "Sending telemetry: device=%u, dest=%u, len=%u, ack=%s",
         _deviceId, message.getMetadata().destinationId, message.getLength(),
         message.getMetadata().requiresAck ? "required" : "not required");

    bool success = _loraHal->sendData(message.getMetadata().destinationId,
                                     message.getPayload(),
                                     message.getLength(),
                                     message.getMetadata().requiresAck);

    if (success) {
        LOGI("LoRaBatchTransmitter", "Successfully queued telemetry message for transmission");
    } else {
        LOGW("LoRaBatchTransmitter", "Failed to queue telemetry message for transmission");
    }

    return success;
}

bool LoRaBatchTransmitter::isReady() const {
    return _loraHal != nullptr;
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
// DEBUG SENSOR IMPLEMENTATIONS
// ============================================================================

class DebugTemperatureSensor : public ISensor {
public:
    DebugTemperatureSensor() {}

    void begin() override {
        // Initialize random seed for more realistic values
        randomSeed(analogRead(0));
    }

    void read(std::vector<SensorReading>& readings) override {
        // Generate a realistic temperature reading (15-35°C)
        float temperature = 20.0f + random(-50, 50) / 10.0f;
        readings.push_back({"temp", temperature, millis()});
    }

    const char* getName() const override {
        return "DebugTemperature";
    }
};

class DebugHumiditySensor : public ISensor {
public:
    DebugHumiditySensor() {}

    void begin() override {
        randomSeed(analogRead(0) + 1);
    }

    void read(std::vector<SensorReading>& readings) override {
        // Generate a realistic humidity reading (40-80%)
        float humidity = 60.0f + random(-200, 200) / 10.0f;
        readings.push_back({"hum", humidity, millis()});
    }

    const char* getName() const override {
        return "DebugHumidity";
    }
};

class DebugBatterySensor : public ISensor {
public:
    DebugBatterySensor() {}

    void begin() override {
        randomSeed(analogRead(0) + 2);
    }

    void read(std::vector<SensorReading>& readings) override {
        // Generate a battery voltage reading (3.0-4.2V)
        float batteryVoltage = 3.7f + random(-7, 5) / 10.0f;
        readings.push_back({"batt", batteryVoltage, millis()});
    }

    const char* getName() const override {
        return "DebugBattery";
    }
};

// ============================================================================
// SENSOR FACTORY
// ============================================================================

namespace SensorFactory {
    std::unique_ptr<ISensor> createDebugTemperatureSensor() {
        return std::make_unique<DebugTemperatureSensor>();
    }

    std::unique_ptr<ISensor> createDebugHumiditySensor() {
        return std::make_unique<DebugHumiditySensor>();
    }

    std::unique_ptr<ISensor> createDebugBatterySensor() {
        return std::make_unique<DebugBatterySensor>();
    }

    // In a real implementation, you would have factory functions here like:
    // std::unique_ptr<ISensor> createUltrasonicSensor(...);
}

#endif // SENSOR_IMPLEMENTATIONS_HPP
