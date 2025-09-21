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

// Manages a collection of sensors and triggers batch transmissions.
class SensorManager {
public:
    SensorManager() : _transmitter(nullptr), _batchSize(1) {}

    void addSensor(std::unique_ptr<ISensor> sensor) {
        _sensors.push_back(std::move(sensor));
    }

    void setTransmitter(SensorBatchTransmitter* transmitter) {
        _transmitter = transmitter;
    }

    void setBatchSize(size_t batchSize) {
        _batchSize = batchSize;
    }

    void update(uint32_t nowMs) {
        // Read from all sensors
        for (const auto& sensor : _sensors) {
            sensor->read(_readings);
        }

        // Attempt to transmit if batch is ready
        if (_transmitter && _readings.size() >= _batchSize) {
            if (_transmitter->transmitBatch(_readings)) {
                _readings.clear(); // Clear ONLY on successful queueing
            }
        }
    }

private:
    std::vector<std::unique_ptr<ISensor>> _sensors;
    SensorBatchTransmitter* _transmitter;
    std::vector<SensorReading> _readings;
    size_t _batchSize;
};
