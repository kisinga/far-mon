// Sensor Interface System - Decoupled and extensible sensor framework
// Provides a unified interface for all sensor types with configurable timing

#pragma once

#include <Arduino.h>
#include <vector>
#include <memory>
#include <functional>

// Sensor data types
enum class SensorDataType {
    TEMPERATURE,
    HUMIDITY,
    DISTANCE,
    WATER_LEVEL,
    FLOW_RATE,
    VOLTAGE,
    CURRENT,
    PRESSURE,
    BOOLEAN,
    CUSTOM
};

// Sensor reading structure
struct SensorReading {
    SensorDataType type;
    const char* name;      // Sensor name/key for transmission
    float value;           // Primary reading value
    const char* unit;      // Unit string (C, %, mm, L/min, etc.)
    uint32_t timestamp;    // Reading timestamp in milliseconds
    bool valid;           // Whether reading is valid

    // Optional secondary values for complex sensors
    std::vector<float> additionalValues;
    std::vector<const char*> additionalNames;

    SensorReading() : type(SensorDataType::CUSTOM), name(""), value(0.0f), unit(""),
                     timestamp(0), valid(false) {}

    SensorReading(SensorDataType t, const char* n, float v, const char* u)
        : type(t), name(n), value(v), unit(u), timestamp(millis()), valid(true) {}
};

// Sensor configuration
struct SensorConfig {
    const char* name;
    uint32_t readIntervalMs;      // How often to read this sensor
    uint32_t batchIntervalMs;     // How often to batch/transmit readings
    bool enabled;
    uint8_t priority;             // Higher priority sensors read first

    SensorConfig() : name(""), readIntervalMs(60000), batchIntervalMs(60000),
                    enabled(false), priority(0) {}

    SensorConfig(const char* n, uint32_t readMs = 60000, uint32_t batchMs = 60000,
                bool en = true, uint8_t pri = 0)
        : name(n), readIntervalMs(readMs), batchIntervalMs(batchMs),
          enabled(en), priority(pri) {}
};

// Abstract sensor interface
class Sensor {
public:
    virtual ~Sensor() = default;

    // Initialize the sensor
    virtual bool begin() = 0;

    // Read sensor data
    virtual SensorReading read() = 0;

    // Get sensor name
    virtual const char* getName() const = 0;

    // Get sensor configuration
    virtual const SensorConfig& getConfig() const = 0;

    // Check if sensor is ready/available
    virtual bool isReady() const = 0;

    // Optional: Get last reading timestamp
    virtual uint32_t getLastReadTime() const { return lastReadTime; }

    // Optional: Force a reading
    virtual bool forceRead() { return false; }

protected:
    uint32_t lastReadTime = 0;
    SensorConfig config;
};

// Sensor factory functions are declared in sensor_implementations.h
// This avoids circular dependencies and keeps the interface clean

// Batch transmission interface
class SensorBatchTransmitter {
public:
    virtual ~SensorBatchTransmitter() = default;

    // Transmit a batch of sensor readings
    virtual bool transmitBatch(const std::vector<SensorReading>& readings) = 0;

    // Check if transmitter is ready
    virtual bool isReady() const = 0;
};

// Sensor manager for coordinating multiple sensors
class SensorManager {
public:
    SensorManager();
    ~SensorManager() = default;

    // Add a sensor to the manager
    bool addSensor(std::unique_ptr<Sensor> sensor);

    // Remove a sensor by name
    bool removeSensor(const char* name);

    // Update sensor readings (called periodically)
    void update(uint32_t currentTime);

    // Set batch transmitter
    void setTransmitter(std::unique_ptr<SensorBatchTransmitter> transmitter);

    // Get all current readings
    std::vector<SensorReading> getAllReadings() const;

    // Force update of all sensors
    void forceUpdateAll();

    // Get sensor count
    size_t getSensorCount() const { return sensors.size(); }

    // Get sensor by name
    Sensor* getSensor(const char* name) const;

private:
    std::vector<std::unique_ptr<Sensor>> sensors;
    std::unique_ptr<SensorBatchTransmitter> transmitter;
    uint32_t lastBatchTime = 0;
    uint32_t batchIntervalMs = 60000; // Default 1 minute batch interval

    // Sort sensors by priority for reading order
    void sortSensorsByPriority();
};

// Helper macros for sensor registration
#define DECLARE_SENSOR_TYPE(TypeName) \
    class TypeName##Sensor : public Sensor { \
    public: \
        TypeName##Sensor(const SensorConfig& cfg) : config(cfg) {} \
        bool begin() override; \
        SensorReading read() override; \
        const char* getName() const override { return config.name; } \
        const SensorConfig& getConfig() const override { return config; } \
        bool isReady() const override; \
    private: \
        SensorConfig config; \
    };

#define IMPLEMENT_SENSOR_TYPE(TypeName, ...) \
    bool TypeName##Sensor::begin() { __VA_ARGS__ } \
    SensorReading TypeName##Sensor::read() { __VA_ARGS__ } \
    bool TypeName##Sensor::isReady() const { __VA_ARGS__ }
