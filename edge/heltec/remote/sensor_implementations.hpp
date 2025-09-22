#ifndef SENSOR_IMPLEMENTATIONS_HPP
#define SENSOR_IMPLEMENTATIONS_HPP

#include <Arduino.h>
#include "sensor_interface.hpp"
#include "lib/core_logger.h"
#include "lib/hal_lora.h"
#include "lib/common_message_types.h"
#include <vector>
#include "lib/hal_persistence.h" // Include the new persistence HAL
#include "lib/svc_battery.h" // Include for IBatteryService

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
        payload += readings[i].type;
        payload += ":";
        if (isnan(readings[i].value)) {
            payload += "disabled";
        } else {
            payload += String(readings[i].value, 2);
        }
        if (i < readings.size() - 1) {
            payload += ",";
        }
    }
    return payload;
}

// ============================================================================
// YF-S201 Water Flow Sensor Implementation
// ============================================================================

class YFS201WaterFlowSensor : public ISensor {
public:
    YFS201WaterFlowSensor(uint8_t pin, IPersistenceHal* persistence, const char* persistence_namespace);
    ~YFS201WaterFlowSensor();

    void begin() override;
    void read(std::vector<SensorReading>& readings) override;
    const char* getName() const override { return "YFS201WaterFlow"; }

    // Public static method to check and clear the interrupt flag
    static bool getAndClearInterruptFlag() {
        if (_interruptFired) {
            _interruptFired = false;
            return true;
        }
        return false;
    }

    // Public method for external task to save the total volume
    void saveTotalVolume();

private:
    const uint8_t _pin;
    IPersistenceHal* _persistence;
    const char* _persistence_namespace;
    
    // Pulse counting
    static void IRAM_ATTR pulseCounter();
    static volatile uint32_t _pulseCount;
    static volatile bool _interruptFired; // Flag for debug logging
    
    // Last read time for flow rate calculation
    unsigned long _lastReadTimeMs = 0;
    
    // Total volume tracking
    uint32_t _totalPulses = 0;

    // YF-S201 constant: pulses per liter (can be calibrated)
    const float PULSES_PER_LITER = 450.0f;
};

// Define static members
volatile uint32_t YFS201WaterFlowSensor::_pulseCount = 0;
volatile bool YFS201WaterFlowSensor::_interruptFired = false;

YFS201WaterFlowSensor::YFS201WaterFlowSensor(uint8_t pin, IPersistenceHal* persistence, const char* persistence_namespace)
    : _pin(pin), _persistence(persistence), _persistence_namespace(persistence_namespace) {
}

YFS201WaterFlowSensor::~YFS201WaterFlowSensor() {
    if (_pin != 0) {
        detachInterrupt(digitalPinToInterrupt(_pin));
    }
}

void YFS201WaterFlowSensor::begin() {
    if (_persistence) {
        _persistence->begin(_persistence_namespace);
        _totalPulses = _persistence->loadU32("totalPulses");
        _persistence->end();
        LOGD(getName(), "Loaded total pulses from memory: %u", _totalPulses);
    }

    pinMode(_pin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(_pin), pulseCounter, FALLING);
    _lastReadTimeMs = millis();
}

void IRAM_ATTR YFS201WaterFlowSensor::pulseCounter() {
    _pulseCount++;
    _interruptFired = true;
}

void YFS201WaterFlowSensor::read(std::vector<SensorReading>& readings) {
    uint32_t currentPulses = 0;
    
    // Safely read and reset the pulse counter
    noInterrupts();
    currentPulses = _pulseCount;
    _pulseCount = 0;
    interrupts();

    unsigned long currentTimeMs = millis();
    unsigned long elapsedTimeMs = currentTimeMs - _lastReadTimeMs;
    _lastReadTimeMs = currentTimeMs;

    // Calculate flow rate in Liters/Minute
    float flowRateLPM = 0.0f;
    if (elapsedTimeMs > 0) {
        // Frequency (Hz) = pulses / (time in seconds)
        float frequency = (float)currentPulses / (elapsedTimeMs / 1000.0f);
        // YF-S201 specific conversion from frequency to L/min (Q = F/7.5, but pulses/liter is more direct)
        // We'll use our PULSES_PER_LITER constant for better accuracy.
        // Flow rate (L/s) = frequency / PULSES_PER_LITER
        // Flow rate (L/min) = (frequency * 60) / PULSES_PER_LITER
        flowRateLPM = (frequency * 60.0f) / PULSES_PER_LITER;
    }

    // Update total volume
    _totalPulses += currentPulses;
    float totalVolumeLiters = (float)_totalPulses / PULSES_PER_LITER;

    readings.push_back({"flow_rate", flowRateLPM, currentTimeMs});
    readings.push_back({"total_vol", totalVolumeLiters, currentTimeMs});
    
    LOGD(getName(), "Pulses: %u, Rate: %.2f L/min, Total: %.2f L", currentPulses, flowRateLPM, totalVolumeLiters);
}

void YFS201WaterFlowSensor::saveTotalVolume() {
    if (_persistence) {
        _persistence->begin(_persistence_namespace);
        bool success = _persistence->saveU32("totalPulses", _totalPulses);
        _persistence->end();
        if (success) {
            LOGD(getName(), "Successfully saved total pulses: %u", _totalPulses);
        } else {
            LOGW(getName(), "Failed to save total pulses.");
        }
    }
}

// ============================================================================
// REAL SENSOR IMPLEMENTATIONS
// ============================================================================

class BatteryMonitorSensor : public ISensor {
public:
    BatteryMonitorSensor(IBatteryService* batteryService) : _batteryService(batteryService) {}

    void begin() override {
        // Nothing to initialize, service is managed externally
    }

    void read(std::vector<SensorReading>& readings) override {
        if (_batteryService) {
            readings.push_back({"batt_pct", (float)_batteryService->getBatteryPercent(), millis()});
        }
    }

    const char* getName() const override {
        return "BatteryMonitor";
    }

private:
    IBatteryService* _batteryService;
};

// ============================================================================
// SENSOR FACTORY
// ============================================================================

namespace SensorFactory {
    std::shared_ptr<YFS201WaterFlowSensor> createYFS201WaterFlowSensor(uint8_t pin, IPersistenceHal* persistence, const char* persistence_namespace) {
        return std::make_shared<YFS201WaterFlowSensor>(pin, persistence, persistence_namespace);
    }

    std::shared_ptr<ISensor> createBatteryMonitorSensor(IBatteryService* batteryService) {
        return std::make_shared<BatteryMonitorSensor>(batteryService);
    }

    // In a real implementation, you would have factory functions here like:
    // std::unique_ptr<ISensor> createUltrasonicSensor(...);
}

#endif // SENSOR_IMPLEMENTATIONS_HPP
