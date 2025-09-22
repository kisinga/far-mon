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
    virtual void addReading(const SensorReading& reading) = 0;
    virtual void update(uint32_t nowMs) = 0;
    virtual bool isReady() const = 0;
};

// Manages a collection of sensors and triggers batch transmissions.
class SensorManager {
public:
    SensorManager() : _transmitter(nullptr) {}

    void addSensor(std::shared_ptr<ISensor> sensor) {
        if (sensor) {
            sensor->begin(); // Initialize the sensor when it's added
            _sensors.push_back(sensor);
        }
    }

    void setTransmitter(SensorBatchTransmitter* transmitter) {
        _transmitter = transmitter;
    }

    void update(uint32_t nowMs) {
        // Read from all sensors and pass readings to the transmitter
        if (!_transmitter) return;

        std::vector<SensorReading> readings;
        for (const auto& sensor : _sensors) {
            sensor->read(readings);
        }
        for (const auto& reading : readings) {
            _transmitter->addReading(reading);
        }
    }

private:
    std::vector<std::shared_ptr<ISensor>> _sensors;
    SensorBatchTransmitter* _transmitter;
};
