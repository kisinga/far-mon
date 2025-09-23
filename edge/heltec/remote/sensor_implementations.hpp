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
#include <limits> // For NaN
#include "ArduinoJson.h" 
#include "lib/core_config.h" // For RemoteConfig
#include "lib/telemetry_keys.h" // For consistent keys

// Forward-declare the config struct to avoid circular dependency
struct RemoteConfig;

// ============================================================================
// LoRa Batch Transmitter Implementation
// ============================================================================

class LoRaBatchTransmitter : public SensorBatchTransmitter {
public:
    LoRaBatchTransmitter(ILoRaHal* loraHal, const RemoteConfig& config);
    bool queueBatch(const std::vector<SensorReading>& readings) override;
    void update(uint32_t nowMs) override;
    bool isReady() const override;

private:
    String formatReadings(const std::vector<SensorReading>& readings);
    ILoRaHal* _loraHal;
    const RemoteConfig& _config;
    std::vector<SensorReading> _readings;
    uint32_t _lastTxTimeMs = 0;
};

LoRaBatchTransmitter::LoRaBatchTransmitter(ILoRaHal* loraHal, const RemoteConfig& config)
    : _loraHal(loraHal), _config(config) {}

bool LoRaBatchTransmitter::queueBatch(const std::vector<SensorReading>& readings) {
    if (!_readings.empty()) {
        LOGD("LoRaBatchTransmitter", "Buffer is not empty, refusing to queue new batch.");
        return false; // Refuse to overwrite a batch that hasn't been sent
    }
    _readings = readings;
    return true;
}

void LoRaBatchTransmitter::update(uint32_t nowMs) {
    if (_readings.empty()) {
        return; // Nothing to send
    }

    // Do not attempt to transmit if not connected to the master.
    if (!_loraHal || !_loraHal->isConnected()) {
        LOGD("LoRaBatchTransmitter", "Not connected, deferring transmission of %u readings.", _readings.size());
        return;
    }
    
    // Respect the HAL's busy state.
    if (!_loraHal->isReadyForTx()) {
        LOGD("LoRaBatchTransmitter", "LoRa HAL is busy, deferring transmission of %u readings.", _readings.size());
        return;
    }

    String payload = formatReadings(_readings);

    LOGD("LoRaBatchTransmitter", "Formatted %u sensor readings into payload: '%s'",
         (unsigned)_readings.size(), payload.c_str());

    if (payload.length() == 0) {
        LOGW("LoRaBatchTransmitter", "Failed to format sensor readings for transmission");
        _readings.clear(); // Clear the buffer to prevent repeated failures
        return;
    }
    
    // The underlying LoRaComm library has a max payload size. We must respect it.
    if (payload.length() > LORA_COMM_MAX_PAYLOAD) {
        LOGW("LoRaBatchTransmitter", "Payload of %d bytes exceeds max of %d. Dropping batch.", payload.length(), LORA_COMM_MAX_PAYLOAD);
        _readings.clear();
        return;
    }

    // Pass the RAW payload string to the HAL
    bool success = _loraHal->sendData(_config.masterNodeId,
                                     (const uint8_t*)payload.c_str(),
                                     (uint8_t)payload.length(),
                                     true); // require ACK

    if (success) {
        LOGI("LoRaBatchTransmitter", "Successfully queued telemetry message for transmission");
        _readings.clear(); // Clear the buffer ONLY on successful queueing
    } else {
        LOGW("LoRaBatchTransmitter", "Failed to queue telemetry message for transmission");
        // Do not clear the buffer, we will retry on the next update.
    }
}

bool LoRaBatchTransmitter::isReady() const {
    return _loraHal != nullptr;
}

String LoRaBatchTransmitter::formatReadings(const std::vector<SensorReading>& readings) {
    String payload = "";
    for (size_t i = 0; i < readings.size(); ++i) {
        if (i > 0) payload += ",";
        payload += readings[i].type;
        payload += ":";
        if (isnan(readings[i].value)) {
            payload += "nan";
        } else {
            // Use integer for counters, float for others
            if (strcmp(readings[i].type, TelemetryKeys::PulseDelta) == 0 ||
                strcmp(readings[i].type, TelemetryKeys::BatteryPercent) == 0 ||
                strcmp(readings[i].type, TelemetryKeys::ErrorCount) == 0 ||
                strcmp(readings[i].type, TelemetryKeys::TimeSinceReset) == 0) {
                payload += String((int)readings[i].value);
            } else {
                payload += String(readings[i].value, 2);
            }
        }
    }
    return payload;
}

// ============================================================================
// YF-S201 Water Flow Sensor Implementation
// ============================================================================

class YFS201WaterFlowSensor : public ISensor {
public:
    YFS201WaterFlowSensor(uint8_t pin, bool enabled, IPersistenceHal* persistence, const char* persistence_namespace);
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
    
    // Public method to reset the volume counter
    void resetTotalVolume();

private:
    const uint8_t _pin;
    const bool _enabled;
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
    static constexpr float PULSES_PER_LITER = 450.0f;
};

// Define static members
volatile uint32_t YFS201WaterFlowSensor::_pulseCount = 0;
volatile bool YFS201WaterFlowSensor::_interruptFired = false;

YFS201WaterFlowSensor::YFS201WaterFlowSensor(uint8_t pin, bool enabled, IPersistenceHal* persistence, const char* persistence_namespace)
    : _pin(pin), _enabled(enabled), _persistence(persistence), _persistence_namespace(persistence_namespace) {
}

YFS201WaterFlowSensor::~YFS201WaterFlowSensor() {
    if (_enabled && _pin != 0) {
        detachInterrupt(digitalPinToInterrupt(_pin));
    }
}

void YFS201WaterFlowSensor::begin() {
    if (!_enabled) return;

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
    unsigned long currentTimeMs = millis();

    if (!_enabled) {
        readings.push_back({TelemetryKeys::PulseDelta, std::numeric_limits<float>::quiet_NaN(), currentTimeMs});
        readings.push_back({TelemetryKeys::TotalVolume, std::numeric_limits<float>::quiet_NaN(), currentTimeMs});
        return;
    }

    // Atomically get and reset the pulse count
    noInterrupts();
    unsigned long currentPulses = _pulseCount;
    _pulseCount = 0;
    interrupts();

    _lastReadTimeMs = currentTimeMs;

    // Report the raw pulse delta. The relay will calculate rate.
    readings.push_back({TelemetryKeys::PulseDelta, (float)currentPulses, currentTimeMs});

    // We still report total volume for the remote's display and persistence.
    _totalPulses += currentPulses;
    float totalVolumeLiters = (float)_totalPulses / YFS201WaterFlowSensor::PULSES_PER_LITER;
    readings.push_back({TelemetryKeys::TotalVolume, totalVolumeLiters, currentTimeMs});
    
    LOGD(getName(), "Read %u pulses", currentPulses);
}

void YFS201WaterFlowSensor::resetTotalVolume() {
    if (!_enabled) return;
    
    LOGI(getName(), "Resetting total volume. Old value (pulses): %u", _totalPulses);
    _totalPulses = 0;
    saveTotalVolume(); // Persist the reset immediately
}

void YFS201WaterFlowSensor::saveTotalVolume() {
    if (!_enabled || !_persistence) {
        return;
    }
    
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
    BatteryMonitorSensor(IBatteryService* batteryService, bool enabled) 
      : _batteryService(batteryService), _enabled(enabled) {}

    void begin() override {
        // Nothing to initialize, service is managed externally
    }

    void read(std::vector<SensorReading>& readings) override {
        if (_enabled && _batteryService) {
            readings.push_back({TelemetryKeys::BatteryPercent, (float)_batteryService->getBatteryPercent(), millis()});
        } else {
            readings.push_back({TelemetryKeys::BatteryPercent, std::numeric_limits<float>::quiet_NaN(), millis()});
        }
    }

    const char* getName() const override {
        return "BatteryMonitor";
    }

private:
    IBatteryService* _batteryService;
    const bool _enabled;
};

// ============================================================================
// SENSOR FACTORY
// ============================================================================

namespace SensorFactory {
    std::shared_ptr<YFS201WaterFlowSensor> createYFS201WaterFlowSensor(uint8_t pin, bool enabled, IPersistenceHal* persistence, const char* persistence_namespace) {
        return std::make_shared<YFS201WaterFlowSensor>(pin, enabled, persistence, persistence_namespace);
    }

    std::shared_ptr<ISensor> createBatteryMonitorSensor(IBatteryService* batteryService, bool enabled) {
        return std::make_shared<BatteryMonitorSensor>(batteryService, enabled);
    }

    // In a real implementation, you would have factory functions here like:
    // std::unique_ptr<ISensor> createUltrasonicSensor(...);
}

#endif // SENSOR_IMPLEMENTATIONS_HPP
