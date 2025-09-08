// Specific Sensor Implementations
// Concrete implementations for various sensor types

#pragma once

#include "sensor_interface.h"

// ============================================================================
// Ultrasonic Distance Sensor (JSN-SR04T)
// ============================================================================

class UltrasonicSensor : public Sensor {
public:
    UltrasonicSensor(const SensorConfig& cfg, uint8_t trigPin, uint8_t echoPin);
    ~UltrasonicSensor() override = default;

    bool begin() override;
    SensorReading read() override;
    const char* getName() const override { return config.name; }
    const SensorConfig& getConfig() const override { return config; }
    bool isReady() const override;

    // Force a reading (useful for calibration)
    bool forceRead() override;

private:
    uint8_t trigPin;
    uint8_t echoPin;
    SensorConfig config;

    // Measure distance using ultrasonic sensor
    float measureDistance();
};

// ============================================================================
// Water Level Sensor (Float Switch)
// ============================================================================

class WaterLevelSensor : public Sensor {
public:
    WaterLevelSensor(const SensorConfig& cfg, uint8_t sensorPin, bool normallyOpen = true);
    ~WaterLevelSensor() override = default;

    bool begin() override;
    SensorReading read() override;
    const char* getName() const override { return config.name; }
    const SensorConfig& getConfig() const override { return config; }
    bool isReady() const override;

private:
    uint8_t sensorPin;
    bool normallyOpen;
    SensorConfig config;
};

// ============================================================================
// Water Flow Sensor (YF-G1)
// ============================================================================

class WaterFlowSensor : public Sensor {
public:
    WaterFlowSensor(const SensorConfig& cfg, uint8_t sensorPin);
    ~WaterFlowSensor() override = default;

    bool begin() override;
    SensorReading read() override;
    const char* getName() const override { return config.name; }
    const SensorConfig& getConfig() const override { return config; }
    bool isReady() const override;

    // Get total flow since last reset
    float getTotalFlow() const { return totalFlow; }

    // Reset flow counter
    void resetFlow() { totalFlow = 0.0f; }

private:
    uint8_t sensorPin;
    SensorConfig config;
    volatile uint32_t pulseCount;
    float totalFlow; // Total flow in liters
    uint32_t lastPulseTime;

    // Interrupt handler for pulse counting
    static void IRAM_ATTR pulseInterrupt(void* arg);

    // Convert pulses to flow rate
    float calculateFlowRate(uint32_t pulses, uint32_t timeMs);
};

// ============================================================================
// RS485 Communication Interface
// ============================================================================

class RS485Sensor : public Sensor {
public:
    RS485Sensor(const SensorConfig& cfg, HardwareSerial& serial, uint8_t rePin, uint8_t dePin);
    ~RS485Sensor() override = default;

    bool begin() override;
    SensorReading read() override;
    const char* getName() const override { return config.name; }
    const SensorConfig& getConfig() const override { return config; }
    bool isReady() const override;

    // Send command and wait for response
    bool sendCommand(const uint8_t* cmd, size_t cmdLen, uint8_t* response, size_t& responseLen, uint32_t timeoutMs = 1000);

protected:
    HardwareSerial& serial;
    uint8_t rePin;  // Receive Enable
    uint8_t dePin;  // Driver Enable
    SensorConfig config;

    // Set RS485 to transmit mode
    void setTransmitMode();

    // Set RS485 to receive mode
    void setReceiveMode();
};

// ============================================================================
// Temperature/Humidity Sensor (DHT-style)
// ============================================================================

class TemperatureHumiditySensor : public Sensor {
public:
    TemperatureHumiditySensor(const SensorConfig& cfg, uint8_t dataPin);
    ~TemperatureHumiditySensor() override = default;

    bool begin() override;
    SensorReading read() override;
    const char* getName() const override { return config.name; }
    const SensorConfig& getConfig() const override { return config; }
    bool isReady() const override;

private:
    uint8_t dataPin;
    SensorConfig config;

    // Read temperature and humidity
    bool readDHT(float& temperature, float& humidity);
};

// ============================================================================
// LoRa Batch Transmitter Implementation
// ============================================================================

class LoRaBatchTransmitter : public SensorBatchTransmitter {
public:
    LoRaBatchTransmitter(class LoRaComm* loraComm, const char* deviceId);
    ~LoRaBatchTransmitter() override = default;

    bool transmitBatch(const std::vector<SensorReading>& readings) override;
    bool isReady() const override;

private:
    class LoRaComm* loraComm;
    const char* deviceId;

    // Format readings into transmission string
    String formatReadings(const std::vector<SensorReading>& readings);

    // Calculate maximum payload size
    size_t getMaxPayloadSize() const;
};

// ============================================================================
// Sensor Factory Implementation
// ============================================================================

namespace SensorFactory {
    // Create ultrasonic sensor
    std::unique_ptr<Sensor> createUltrasonicSensor(const SensorConfig& config,
                                                  uint8_t trigPin, uint8_t echoPin);

    // Create water level sensor
    std::unique_ptr<Sensor> createWaterLevelSensor(const SensorConfig& config,
                                                  uint8_t sensorPin, bool normallyOpen = true);

    // Create water flow sensor
    std::unique_ptr<Sensor> createWaterFlowSensor(const SensorConfig& config,
                                                 uint8_t sensorPin);

    // Create RS485 sensor
    std::unique_ptr<Sensor> createRS485Sensor(const SensorConfig& config,
                                             HardwareSerial& serial, uint8_t rePin, uint8_t dePin);

    // Create temperature/humidity sensor
    std::unique_ptr<Sensor> createTempHumiditySensor(const SensorConfig& config,
                                                    uint8_t dataPin);
}
