#pragma once

#include <stdint.h>
#include <vector>
#include <memory> // For std::unique_ptr

// Forward declaration
class SensorBatchTransmitter;

// Basic structure for a sensor reading
struct SensorReading {
    const char* type; // e.g., "temp", "hum"
    float value;
    uint32_t timestamp;
};

// Interface for all sensors
class ISensor {
public:
    virtual ~ISensor() = default;
    virtual void begin() = 0;
    virtual void read(std::vector<SensorReading>& readings) = 0;
    virtual const char* getName() const = 0;
};

// Interface for transmitting batches of sensor data
class SensorBatchTransmitter {
public:
    virtual ~SensorBatchTransmitter() = default;
    virtual bool transmitBatch(const std::vector<SensorReading>& readings) = 0;
    virtual bool isReady() const = 0;
};

// Manages a collection of sensors and coordinates data transmission
class SensorManager {
public:
    void addSensor(std::unique_ptr<ISensor> sensor) {
        _sensors.push_back(std::move(sensor));
    }

    void setTransmitter(SensorBatchTransmitter* transmitter) {
        _transmitter = transmitter;
    }

    void update(uint32_t nowMs) {
        std::vector<SensorReading> readings;
        for (auto& sensor : _sensors) {
            sensor->read(readings);
        }

        if (_transmitter && !_transmitter->isReady()) {
            return;
        }

        // This is a simplified transmission trigger. A real implementation
        // would have more sophisticated logic (e.g., configurable intervals).
        if (_transmitter && nowMs - _lastTransmissionMs > 5000) {
            _transmitter->transmitBatch(readings);
            _lastTransmissionMs = nowMs;
        }
    }

    size_t getSensorCount() const {
        return _sensors.size();
    }

private:
    std::vector<std::unique_ptr<ISensor>> _sensors;
    SensorBatchTransmitter* _transmitter = nullptr;
    uint32_t _lastTransmissionMs = 0;
};
