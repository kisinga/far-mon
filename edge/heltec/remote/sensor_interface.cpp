// Sensor Interface Implementation
// Provides concrete implementations for sensor management

#include "sensor_interface.h"
#include <algorithm>

// SensorManager implementation
SensorManager::SensorManager() {
    lastBatchTime = millis();
}

bool SensorManager::addSensor(std::unique_ptr<Sensor> sensor) {
    if (!sensor) return false;

    // Check if sensor with same name already exists
    for (const auto& existing : sensors) {
        if (strcmp(existing->getName(), sensor->getName()) == 0) {
            return false; // Duplicate name
        }
    }

    sensors.push_back(std::move(sensor));
    sortSensorsByPriority();
    return true;
}

bool SensorManager::removeSensor(const char* name) {
    auto it = std::find_if(sensors.begin(), sensors.end(),
        [name](const std::unique_ptr<Sensor>& sensor) {
            return strcmp(sensor->getName(), name) == 0;
        });

    if (it != sensors.end()) {
        sensors.erase(it);
        return true;
    }
    return false;
}

void SensorManager::update(uint32_t currentTime) {
    bool hasNewReadings = false;
    std::vector<SensorReading> batchReadings;

    // Update each sensor
    for (auto& sensor : sensors) {
        if (!sensor->getConfig().enabled) continue;

        // Check if it's time to read this sensor
        uint32_t timeSinceLastRead = currentTime - sensor->getLastReadTime();
        if (timeSinceLastRead >= sensor->getConfig().readIntervalMs) {
            if (sensor->isReady()) {
                SensorReading reading = sensor->read();
                if (reading.valid) {
                    batchReadings.push_back(reading);
                    hasNewReadings = true;
                }
            }
        }
    }

    // Check if it's time to transmit batch
    uint32_t timeSinceLastBatch = currentTime - lastBatchTime;
    if (hasNewReadings && timeSinceLastBatch >= batchIntervalMs) {
        if (transmitter && transmitter->isReady()) {
            transmitter->transmitBatch(batchReadings);
            lastBatchTime = currentTime;
        }
    }
}

void SensorManager::setTransmitter(std::unique_ptr<SensorBatchTransmitter> tx) {
    transmitter = std::move(tx);
}

std::vector<SensorReading> SensorManager::getAllReadings() const {
    std::vector<SensorReading> allReadings;
    for (const auto& sensor : sensors) {
        if (sensor->isReady()) {
            // This is a simplified approach - in practice you'd want to cache readings
            SensorReading reading = sensor->read();
            if (reading.valid) {
                allReadings.push_back(reading);
            }
        }
    }
    return allReadings;
}

void SensorManager::forceUpdateAll() {
    for (auto& sensor : sensors) {
        if (sensor->getConfig().enabled && sensor->isReady()) {
            sensor->forceRead();
        }
    }
}

Sensor* SensorManager::getSensor(const char* name) const {
    for (const auto& sensor : sensors) {
        if (strcmp(sensor->getName(), name) == 0) {
            return sensor.get();
        }
    }
    return nullptr;
}

void SensorManager::sortSensorsByPriority() {
    std::sort(sensors.begin(), sensors.end(),
        [](const std::unique_ptr<Sensor>& a, const std::unique_ptr<Sensor>& b) {
            return a->getConfig().priority > b->getConfig().priority;
        });
}

// SensorFactory functions are implemented in sensor_implementations.cpp
// This keeps the interface clean and avoids circular dependencies
