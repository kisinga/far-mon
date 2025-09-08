// Sensor Implementations
// Concrete implementations for various sensor types

#include "sensor_implementations.h"
#include "../lib/lora_comm.h"
#include <algorithm>

// ============================================================================
// Ultrasonic Sensor Implementation
// ============================================================================

UltrasonicSensor::UltrasonicSensor(const SensorConfig& cfg, uint8_t trigPin, uint8_t echoPin)
    : trigPin(trigPin), echoPin(echoPin), config(cfg) {
}

bool UltrasonicSensor::begin() {
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    digitalWrite(trigPin, LOW);
    return true;
}

SensorReading UltrasonicSensor::read() {
    float distance = measureDistance();
    lastReadTime = millis();

    SensorReading reading(SensorDataType::DISTANCE, config.name, distance, "mm");
    reading.valid = (distance > 0 && distance < 4000); // Valid range 0-4000mm
    return reading;
}

bool UltrasonicSensor::isReady() const {
    return true; // Always ready once initialized
}

bool UltrasonicSensor::forceRead() {
    // Force a reading by triggering measurement
    measureDistance();
    lastReadTime = millis();
    return true;
}

float UltrasonicSensor::measureDistance() {
    // Clear the trigger pin
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);

    // Send 10us pulse to trigger
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    // Read the echo pin
    unsigned long duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout

    // Calculate distance: speed of sound = 343m/s = 0.0343 cm/us
    // Distance = (duration * speed) / 2 (round trip)
    float distance = (duration * 0.0343) / 2.0;

    // Convert cm to mm
    return distance * 10.0;
}

// ============================================================================
// Water Level Sensor Implementation
// ============================================================================

WaterLevelSensor::WaterLevelSensor(const SensorConfig& cfg, uint8_t sensorPin, bool normallyOpen)
    : sensorPin(sensorPin), normallyOpen(normallyOpen), config(cfg) {
}

bool WaterLevelSensor::begin() {
    pinMode(sensorPin, INPUT_PULLUP);
    return true;
}

SensorReading WaterLevelSensor::read() {
    int rawValue = digitalRead(sensorPin);
    lastReadTime = millis();

    // Convert to boolean: true if water detected
    bool waterDetected = normallyOpen ? (rawValue == LOW) : (rawValue == HIGH);

    SensorReading reading(SensorDataType::WATER_LEVEL, config.name,
                         waterDetected ? 1.0f : 0.0f, "");
    reading.valid = true;
    return reading;
}

bool WaterLevelSensor::isReady() const {
    return true;
}

// ============================================================================
// Water Flow Sensor Implementation
// ============================================================================

WaterFlowSensor::WaterFlowSensor(const SensorConfig& cfg, uint8_t sensorPin)
    : sensorPin(sensorPin), config(cfg), pulseCount(0), totalFlow(0.0f), lastPulseTime(0) {
}

bool WaterFlowSensor::begin() {
    pinMode(sensorPin, INPUT_PULLUP);
    attachInterruptArg(digitalPinToInterrupt(sensorPin), pulseInterrupt, this, FALLING);
    return true;
}

SensorReading WaterFlowSensor::read() {
    uint32_t currentTime = millis();
    uint32_t pulses = pulseCount;

    // Calculate flow rate (YF-G1: ~4.5 pulses per liter)
    float flowRate = calculateFlowRate(pulses, currentTime - lastReadTime);

    // Update total flow
    totalFlow += flowRate * ((currentTime - lastReadTime) / 1000.0f / 60.0f); // Convert to liters

    lastReadTime = currentTime;
    pulseCount = 0; // Reset for next measurement period

    SensorReading reading(SensorDataType::FLOW_RATE, config.name, flowRate, "L/min");
    reading.valid = true;

    // Add total flow as additional value
    reading.additionalValues.push_back(totalFlow);
    reading.additionalNames.push_back("total");

    return reading;
}

bool WaterFlowSensor::isReady() const {
    return true;
}

float WaterFlowSensor::calculateFlowRate(uint32_t pulses, uint32_t timeMs) {
    if (timeMs == 0) return 0.0f;

    // YF-G1 specifications: ~4.5 pulses per liter
    // Flow rate calculation: pulses / (pulses_per_liter * time_in_minutes)
    float timeMinutes = timeMs / 60000.0f;
    float pulsesPerLiter = 4.5f; // YF-G1 specification

    return pulses / (pulsesPerLiter * timeMinutes);
}

void IRAM_ATTR WaterFlowSensor::pulseInterrupt(void* arg) {
    WaterFlowSensor* sensor = static_cast<WaterFlowSensor*>(arg);
    if (sensor) {
        sensor->pulseCount++;
    }
}

// ============================================================================
// RS485 Sensor Implementation
// ============================================================================

RS485Sensor::RS485Sensor(const SensorConfig& cfg, HardwareSerial& serial, uint8_t rePin, uint8_t dePin)
    : serial(serial), rePin(rePin), dePin(dePin), config(cfg) {
}

bool RS485Sensor::begin() {
    pinMode(rePin, OUTPUT);
    pinMode(dePin, OUTPUT);
    setReceiveMode();

    // Initialize serial if not already done
    if (!serial) {
        serial.begin(9600); // Default RS485 baud rate
    }

    return true;
}

SensorReading RS485Sensor::read() {
    // This is a base implementation - specific sensors should override
    SensorReading reading(SensorDataType::CUSTOM, config.name, 0.0f, "");
    reading.valid = false;
    lastReadTime = millis();
    return reading;
}

bool RS485Sensor::isReady() const {
    return serial;
}

bool RS485Sensor::sendCommand(const uint8_t* cmd, size_t cmdLen,
                             uint8_t* response, size_t& responseLen, uint32_t timeoutMs) {
    if (!serial) return false;

    // Switch to transmit mode
    setTransmitMode();

    // Send command
    size_t written = serial.write(cmd, cmdLen);
    serial.flush();

    // Switch back to receive mode
    setReceiveMode();

    if (written != cmdLen) return false;

    // Wait for response
    uint32_t startTime = millis();
    size_t bytesRead = 0;

    while (bytesRead < responseLen && (millis() - startTime) < timeoutMs) {
        if (serial.available()) {
            response[bytesRead++] = serial.read();
        }
        delay(1);
    }

    responseLen = bytesRead;
    return bytesRead > 0;
}

void RS485Sensor::setTransmitMode() {
    digitalWrite(rePin, HIGH);
    digitalWrite(dePin, HIGH);
    delay(1); // Allow transceiver to switch
}

void RS485Sensor::setReceiveMode() {
    digitalWrite(rePin, LOW);
    digitalWrite(dePin, LOW);
    delay(1); // Allow transceiver to switch
}

// ============================================================================
// Temperature/Humidity Sensor Implementation
// ============================================================================

TemperatureHumiditySensor::TemperatureHumiditySensor(const SensorConfig& cfg, uint8_t dataPin)
    : dataPin(dataPin), config(cfg) {
}

bool TemperatureHumiditySensor::begin() {
    // Note: This is a simplified implementation
    // Real implementation would initialize DHT sensor
    pinMode(dataPin, INPUT_PULLUP);
    return true;
}

SensorReading TemperatureHumiditySensor::read() {
    float temperature, humidity;

    if (readDHT(temperature, humidity)) {
        lastReadTime = millis();

        // Return temperature as primary reading
        SensorReading reading(SensorDataType::TEMPERATURE, config.name, temperature, "C");
        reading.valid = true;

        // Add humidity as additional value
        reading.additionalValues.push_back(humidity);
        reading.additionalNames.push_back("humidity");

        return reading;
    }

    SensorReading reading(SensorDataType::TEMPERATURE, config.name, 0.0f, "C");
    reading.valid = false;
    return reading;
}

bool TemperatureHumiditySensor::isReady() const {
    return true;
}

bool TemperatureHumiditySensor::readDHT(float& temperature, float& humidity) {
    // Simplified DHT reading implementation
    // In a real implementation, you'd use the DHT library
    // For now, return dummy values for testing
    temperature = 25.0f + random(-50, 51) / 10.0f; // 20.0 - 30.0 C
    humidity = 60.0f + random(-200, 201) / 10.0f;   // 40.0 - 80.0 %
    return true;
}

// ============================================================================
// LoRa Batch Transmitter Implementation
// ============================================================================

LoRaBatchTransmitter::LoRaBatchTransmitter(class LoRaComm* loraComm, const char* deviceId)
    : loraComm(loraComm), deviceId(deviceId) {
}

bool LoRaBatchTransmitter::transmitBatch(const std::vector<SensorReading>& readings) {
    if (!loraComm || readings.empty()) return false;

    String payload = formatReadings(readings);

    if (payload.length() == 0) return false;

    // Send as LoRa message (no ACK for telemetry)
    return loraComm->sendData(1, (const uint8_t*)payload.c_str(),
                             (uint8_t)payload.length(), false);
}

bool LoRaBatchTransmitter::isReady() const {
    return loraComm && loraComm->isConnected();
}

String LoRaBatchTransmitter::formatReadings(const std::vector<SensorReading>& readings) {
    String payload = "id=";
    payload += deviceId;

    for (const auto& reading : readings) {
        if (!reading.valid) continue;

        payload += ",";
        payload += reading.name;
        payload += "=";
        payload += String(reading.value, 2);

        // Add unit if present
        if (reading.unit && strlen(reading.unit) > 0) {
            payload += reading.unit;
        }

        // Add additional values
        for (size_t i = 0; i < reading.additionalValues.size(); ++i) {
            payload += ",";
            if (i < reading.additionalNames.size()) {
                payload += reading.additionalNames[i];
            } else {
                payload += reading.name;
                payload += "_";
                payload += String(i);
            }
            payload += "=";
            payload += String(reading.additionalValues[i], 2);
        }
    }

    return payload;
}

size_t LoRaBatchTransmitter::getMaxPayloadSize() const {
    return 64; // LoRa payload limit
}

// ============================================================================
// Sensor Factory Implementation
// ============================================================================

namespace SensorFactory {

std::unique_ptr<Sensor> createUltrasonicSensor(const SensorConfig& config,
                                             uint8_t trigPin, uint8_t echoPin) {
    return std::make_unique<UltrasonicSensor>(config, trigPin, echoPin);
}

std::unique_ptr<Sensor> createWaterLevelSensor(const SensorConfig& config,
                                             uint8_t sensorPin, bool normallyOpen) {
    return std::make_unique<WaterLevelSensor>(config, sensorPin, normallyOpen);
}

std::unique_ptr<Sensor> createWaterFlowSensor(const SensorConfig& config,
                                            uint8_t sensorPin) {
    return std::make_unique<WaterFlowSensor>(config, sensorPin);
}

std::unique_ptr<Sensor> createRS485Sensor(const SensorConfig& config,
                                        HardwareSerial& serial, uint8_t rePin, uint8_t dePin) {
    return std::make_unique<RS485Sensor>(config, serial, rePin, dePin);
}

std::unique_ptr<Sensor> createTempHumiditySensor(const SensorConfig& config,
                                                uint8_t dataPin) {
    return std::make_unique<TemperatureHumiditySensor>(config, dataPin);
}

} // namespace SensorFactory
