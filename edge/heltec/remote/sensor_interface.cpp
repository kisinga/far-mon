// Sensor Interface Implementation
// Provides concrete implementations for sensor management

#include "sensor_interface.h"
#include "../lib/logger.h"
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

        // Handle sensor initialization and retry logic
        SensorState sensorState = sensor->getState();

        // Initialize uninitialized sensors
        if (sensorState == SensorState::UNINITIALIZED) {
            LOGI("sensor_mgr", "Initializing sensor: %s", sensor->getName());
            if (!sensor->begin()) {
                LOGW("sensor_mgr", "Failed to initialize sensor: %s", sensor->getName());
            }
            continue; // Skip reading on first update after init
        }

        // Retry failed sensors periodically
        if (sensorState == SensorState::FAILED &&
            sensor->shouldRetryInit(currentTime)) {
            LOGI("sensor_mgr", "Retrying sensor: %s (attempt %d/%d)",
                 sensor->getName(), sensor->getFailureCount(), Sensor::getMaxFailures());
            if (sensor->retryInit()) {
                LOGI("sensor_mgr", "Sensor retry successful: %s", sensor->getName());
            } else {
                LOGW("sensor_mgr", "Sensor retry failed: %s", sensor->getName());
            }
        }

        // Skip disabled sensors
        if (sensorState == SensorState::PERMANENTLY_DISABLED) {
            LOGW("sensor_mgr", "Sensor permanently disabled: %s", sensor->getName());
            continue;
        }

        // Check if it's time to read this sensor
        uint32_t timeSinceLastRead = currentTime - sensor->getLastReadTime();
        if (timeSinceLastRead >= sensor->getConfig().readIntervalMs) {
            SensorReading reading = sensor->read();

            // Always add reading to batch (including null readings from failed sensors)
            // This ensures we transmit null values to indicate sensor failure
            batchReadings.push_back(reading);

            if (reading.valid) {
                hasNewReadings = true;
            } else if (sensorState != SensorState::READY) {
                // Log when we get null readings from non-ready sensors
                LOGD("sensor_mgr", "Null reading from sensor: %s (state: %d)",
                     sensor->getName(), static_cast<int>(sensorState));
            }
        }
    }

    // Check if it's time to transmit batch
    uint32_t timeSinceLastBatch = currentTime - lastBatchTime;
    if (!batchReadings.empty() && timeSinceLastBatch >= batchIntervalMs) {
        if (transmitter && transmitter->isReady()) {
            transmitter->transmitBatch(batchReadings);
            lastBatchTime = currentTime;
            LOGI("sensor_mgr", "Transmitted batch with %d readings", batchReadings.size());
        } else if (!transmitter) {
            LOGW("sensor_mgr", "No transmitter configured for sensor batch");
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
