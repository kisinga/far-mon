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
    state = SensorState::INITIALIZING;

    // Configure pins
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    digitalWrite(trigPin, LOW);

    // Test the sensor by attempting a measurement
    delay(100); // Allow sensor to stabilize
    float testDistance = measureDistance();

    // Consider initialization successful if we get a valid reading or reasonable timeout
    bool initSuccess = (testDistance >= 0 && testDistance <= 4000) || (testDistance == -1);
    updateInitState(initSuccess);

    return initSuccess;
}

SensorReading UltrasonicSensor::read() {
    // Return null reading if sensor failed or disabled
    if (state == SensorState::FAILED || state == SensorState::PERMANENTLY_DISABLED) {
        return createNullReading();
    }

    float distance = measureDistance();
    lastReadTime = millis();

    SensorReading reading(SensorDataType::DISTANCE, config.name, distance, "mm");

    // Validate distance reading
    if (distance >= 0 && distance <= 4000) {
        reading.valid = true;
    } else {
        reading.valid = false;
        reading.value = 0.0f; // Null value for invalid readings
    }

    return reading;
}

bool UltrasonicSensor::isReady() const {
    return state == SensorState::READY;
}

bool UltrasonicSensor::forceRead() {
    if (state != SensorState::READY) return false;

    // Force a reading by triggering measurement
    measureDistance();
    lastReadTime = millis();
    return true;
}

bool UltrasonicSensor::retryInit() {
    if (state == SensorState::PERMANENTLY_DISABLED) return false;

    LOGI("ultrasonic", "Retrying initialization for sensor %s", config.name);
    return begin();
}

float UltrasonicSensor::measureDistance() {
    // Clear the trigger pin
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);

    // Send 10us pulse to trigger
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    // Read the echo pin with timeout
    unsigned long duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout

    // Check for timeout (no echo received)
    if (duration == 0) {
        return -1.0f; // Error: no echo received
    }

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
    state = SensorState::INITIALIZING;

    // Configure pin
    pinMode(sensorPin, INPUT_PULLUP);

    // Test the sensor by reading a few times
    delay(10);
    int testRead1 = digitalRead(sensorPin);
    delay(10);
    int testRead2 = digitalRead(sensorPin);

    // Consider initialization successful if we can read from the pin
    // (even if the readings are the same, it means the pin is working)
    bool initSuccess = (testRead1 >= 0 && testRead1 <= 1) && (testRead2 >= 0 && testRead2 <= 1);
    updateInitState(initSuccess);

    return initSuccess;
}

SensorReading WaterLevelSensor::read() {
    // Return null reading if sensor failed or disabled
    if (state == SensorState::FAILED || state == SensorState::PERMANENTLY_DISABLED) {
        return createNullReading();
    }

    int rawValue = digitalRead(sensorPin);
    lastReadTime = millis();

    // Check for invalid digital read
    if (rawValue < 0 || rawValue > 1) {
        SensorReading reading(SensorDataType::WATER_LEVEL, config.name, 0.0f, "");
        reading.valid = false;
        return reading;
    }

    // Convert to boolean: true if water detected
    bool waterDetected = normallyOpen ? (rawValue == LOW) : (rawValue == HIGH);

    SensorReading reading(SensorDataType::WATER_LEVEL, config.name,
                         waterDetected ? 1.0f : 0.0f, "");
    reading.valid = true;
    return reading;
}

bool WaterLevelSensor::isReady() const {
    return state == SensorState::READY;
}

bool WaterLevelSensor::retryInit() {
    if (state == SensorState::PERMANENTLY_DISABLED) return false;

    LOGI("water_level", "Retrying initialization for sensor %s", config.name);
    return begin();
}

// ============================================================================
// Water Flow Sensor Implementation
// ============================================================================

WaterFlowSensor::WaterFlowSensor(const SensorConfig& cfg, uint8_t sensorPin)
    : sensorPin(sensorPin), config(cfg), pulseCount(0), totalFlow(0.0f), lastPulseTime(0) {
}

bool WaterFlowSensor::begin() {
    state = SensorState::INITIALIZING;

    // Configure pin and interrupt
    pinMode(sensorPin, INPUT_PULLUP);

    // Test interrupt attachment
    bool interruptSuccess = (digitalPinToInterrupt(sensorPin) != NOT_AN_INTERRUPT);

    if (interruptSuccess) {
        attachInterruptArg(digitalPinToInterrupt(sensorPin), pulseInterrupt, this, FALLING);
    }

    // Test by checking if we can read from the pin
    delay(10);
    int testRead = digitalRead(sensorPin);
    bool pinSuccess = (testRead >= 0 && testRead <= 1);

    bool initSuccess = interruptSuccess && pinSuccess;
    updateInitState(initSuccess);

    return initSuccess;
}

SensorReading WaterFlowSensor::read() {
    // Return null reading if sensor failed or disabled
    if (state == SensorState::FAILED || state == SensorState::PERMANENTLY_DISABLED) {
        return createNullReading();
    }

    uint32_t currentTime = millis();

    // Prevent division by zero on first read
    if (lastReadTime == 0) {
        lastReadTime = currentTime;
        SensorReading reading(SensorDataType::FLOW_RATE, config.name, 0.0f, "L/min");
        reading.valid = true;
        reading.additionalValues.push_back(totalFlow);
        reading.additionalNames.push_back("total");
        return reading;
    }

    uint32_t pulses = pulseCount;
    uint32_t timeDiff = currentTime - lastReadTime;

    // Calculate flow rate (YF-G1: ~4.5 pulses per liter)
    float flowRate = calculateFlowRate(pulses, timeDiff);

    // Update total flow (convert from L/min to liters)
    totalFlow += flowRate * (timeDiff / 1000.0f / 60.0f);

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
    return state == SensorState::READY;
}

bool WaterFlowSensor::retryInit() {
    if (state == SensorState::PERMANENTLY_DISABLED) return false;

    LOGI("flow", "Retrying initialization for sensor %s", config.name);
    return begin();
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
    state = SensorState::INITIALIZING;

    // Configure control pins
    pinMode(rePin, OUTPUT);
    pinMode(dePin, OUTPUT);
    setReceiveMode();

    // Initialize serial if not already done
    if (!serial) {
        serial.begin(9600); // Default RS485 baud rate
    }

    // Test serial communication
    delay(100); // Allow serial to initialize
    bool serialReady = serial;

    // Test basic communication by checking if we can send/receive
    if (serialReady) {
        setTransmitMode();
        serial.write((uint8_t)0x00); // Test byte
        serial.flush();
        setReceiveMode();

        // Check if serial is still available after test
        serialReady = serial && (serial.availableForWrite() > 0);
    }

    updateInitState(serialReady);
    return serialReady;
}

SensorReading RS485Sensor::read() {
    // Return null reading if sensor failed or disabled
    if (state == SensorState::FAILED || state == SensorState::PERMANENTLY_DISABLED) {
        return createNullReading();
    }

    // This is a base implementation - specific RS485 sensors should override
    // For now, return a basic null reading
    SensorReading reading(SensorDataType::CUSTOM, config.name, 0.0f, "");
    reading.valid = false;
    lastReadTime = millis();
    return reading;
}

bool RS485Sensor::isReady() const {
    return state == SensorState::READY && serial;
}

bool RS485Sensor::retryInit() {
    if (state == SensorState::PERMANENTLY_DISABLED) return false;

    LOGI("rs485", "Retrying initialization for sensor %s", config.name);
    return begin();
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
    state = SensorState::INITIALIZING;

    // Configure data pin
    pinMode(dataPin, INPUT_PULLUP);

    // Test the sensor by attempting to read
    delay(100); // Allow sensor to stabilize
    float testTemp, testHumidity;
    bool testSuccess = readDHT(testTemp, testHumidity);

    // Consider initialization successful if we get any reading (even if invalid)
    // This means the pin is working, sensor may just need time to stabilize
    bool initSuccess = (testTemp >= -50 && testTemp <= 100) && (testHumidity >= 0 && testHumidity <= 100);
    updateInitState(initSuccess);

    return initSuccess;
}

SensorReading TemperatureHumiditySensor::read() {
    // Return null reading if sensor failed or disabled
    if (state == SensorState::FAILED || state == SensorState::PERMANENTLY_DISABLED) {
        return createNullReading();
    }

    float temperature, humidity;

    if (readDHT(temperature, humidity)) {
        lastReadTime = millis();

        // Validate readings
        bool tempValid = (temperature >= -50 && temperature <= 100);
        bool humidityValid = (humidity >= 0 && humidity <= 100);

        SensorReading reading(SensorDataType::TEMPERATURE, config.name,
                             tempValid ? temperature : 0.0f, "C");
        reading.valid = tempValid && humidityValid;

        // Add humidity as additional value
        reading.additionalValues.push_back(humidityValid ? humidity : 0.0f);
        reading.additionalNames.push_back("humidity");

        return reading;
    }

    // Return invalid reading if DHT read failed
    SensorReading reading(SensorDataType::TEMPERATURE, config.name, 0.0f, "C");
    reading.valid = false;
    reading.additionalValues.push_back(0.0f);
    reading.additionalNames.push_back("humidity");
    return reading;
}

bool TemperatureHumiditySensor::isReady() const {
    return state == SensorState::READY;
}

bool TemperatureHumiditySensor::retryInit() {
    if (state == SensorState::PERMANENTLY_DISABLED) return false;

    LOGI("temp_humidity", "Retrying initialization for sensor %s", config.name);
    return begin();
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
        payload += ",";
        payload += reading.name;
        payload += "=";

        if (reading.valid) {
            // Valid reading
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
        } else {
            // Invalid/null reading from failed sensor
            payload += "null";
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
