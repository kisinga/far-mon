# MQTT Reliability Guide

## Overview

This document outlines the comprehensive MQTT reliability improvements implemented in the farm monitoring system. The enhanced MQTT implementation provides robust connection management, message queuing, and detailed monitoring capabilities.

## Key Improvements

### 1. Enhanced Connection Management

#### **Configurable Timeouts**

- **Connection Timeout**: Increased from 1000ms to 15000ms for better reliability
- **Keep Alive**: Configurable keep-alive interval (default: 30 seconds)
- **Retry Logic**: Exponential backoff with jitter to prevent thundering herd

#### **Connection States**

The system now tracks detailed connection states:

- `Disconnected`: Not connected to broker
- `Connecting`: Attempting to connect
- `Connected`: Successfully connected
- `Reconnecting`: Attempting to reconnect after disconnection
- `Failed`: Max retry attempts reached

### 2. Message Queuing System

#### **Reliability Features**

- **Automatic Queuing**: Messages are queued when MQTT is disconnected
- **Queue Size**: Configurable queue size (default: 50 messages)
- **Message Expiry**: Old messages (>5 minutes) are automatically dropped
- **Retry Logic**: Failed messages are retried up to 3 times

#### **Queue Management**

- **FIFO Processing**: Messages are processed in first-in-first-out order
- **Batch Processing**: Up to 5 messages processed per update cycle
- **Memory Management**: Automatic cleanup of expired messages

### 3. Enhanced Error Handling

#### **Detailed Error Reporting**

The system now provides specific error messages for common MQTT issues:

- Connection timeout
- Connection refused
- Unacceptable protocol version
- Identifier rejected
- Server unavailable
- Bad username/password
- Not authorized

#### **Retry Strategy**

- **Exponential Backoff**: Retry intervals increase exponentially
- **Jitter**: Random component prevents synchronized retries
- **Max Attempts**: Configurable maximum retry attempts (default: 10)
- **Recovery**: Automatic reset after max attempts reached

### 4. Monitoring and Statistics

#### **Connection Monitoring**

- **Retry Attempts**: Track number of connection attempts
- **Last Connection Time**: Timestamp of successful connection
- **Connection State**: Current connection state
- **Queue Status**: Number of queued messages

#### **Publish Statistics**

- **Successful Publishes**: Count of successful message publications
- **Failed Publishes**: Count of failed message publications
- **Queue Utilization**: Current queue usage

## Configuration

### MQTT Configuration Structure

```cpp
struct MqttConfig {
    // Basic settings
    bool enableMqtt = false;
    const char* brokerHost = nullptr;
    uint16_t brokerPort = 1883;
    const char* clientId = nullptr;
    const char* username = nullptr;
    const char* password = nullptr;
    const char* baseTopic = nullptr;
    const char* deviceTopic = nullptr;
    uint8_t qos = 0;
    bool retain = false;

    // Reliability settings
    uint32_t connectionTimeoutMs = 10000;    // Connection timeout
    uint32_t keepAliveMs = 30;               // Keep alive interval
    uint32_t retryIntervalMs = 5000;         // Base retry interval
    uint32_t maxRetryIntervalMs = 60000;     // Maximum retry interval
    uint8_t maxRetryAttempts = 10;           // Maximum retry attempts
    uint16_t maxQueueSize = 50;              // Maximum queued messages
    bool enableMessageQueue = true;          // Enable message queuing
};
```

### Recommended Settings

#### **For Production Use**

```cpp
cfg.communication.mqtt.connectionTimeoutMs = 15000;    // 15 second timeout
cfg.communication.mqtt.keepAliveMs = 30;               // 30 second keep alive
cfg.communication.mqtt.retryIntervalMs = 5000;         // 5 second base retry
cfg.communication.mqtt.maxRetryIntervalMs = 60000;     // 1 minute max retry
cfg.communication.mqtt.maxRetryAttempts = 10;          // 10 retry attempts
cfg.communication.mqtt.maxQueueSize = 50;              // 50 message queue
cfg.communication.mqtt.enableMessageQueue = true;      // Enable queuing
```

#### **For Development/Testing**

```cpp
cfg.communication.mqtt.connectionTimeoutMs = 5000;     // 5 second timeout
cfg.communication.mqtt.keepAliveMs = 15;               // 15 second keep alive
cfg.communication.mqtt.retryIntervalMs = 2000;         // 2 second base retry
cfg.communication.mqtt.maxRetryIntervalMs = 30000;     // 30 second max retry
cfg.communication.mqtt.maxRetryAttempts = 5;           // 5 retry attempts
cfg.communication.mqtt.maxQueueSize = 20;              // 20 message queue
cfg.communication.mqtt.enableMessageQueue = true;      // Enable queuing
```

## Usage Examples

### Basic MQTT Publishing

```cpp
// Publish sensor data
uint8_t sensorData[] = {0x01, 0x02, 0x03, 0x04};
bool success = mqttPublisher.publish("sensor/1", sensorData, sizeof(sensorData));

if (success) {
    Serial.println("Message published or queued successfully");
} else {
    Serial.println("Message failed to publish or queue");
}
```

### Monitoring Connection Status

```cpp
// Check connection state
MqttConnectionState state = mqttPublisher.getConnectionState();
switch (state) {
    case MqttConnectionState::Connected:
        Serial.println("MQTT connected");
        break;
    case MqttConnectionState::Reconnecting:
        Serial.printf("MQTT reconnecting (attempt %u)\n", mqttPublisher.getRetryAttempts());
        break;
    case MqttConnectionState::Failed:
        Serial.println("MQTT connection failed");
        break;
    default:
        Serial.println("MQTT disconnected");
        break;
}
```

### Queue Management

```cpp
// Check queue status
uint16_t queueCount = mqttPublisher.getQueuedMessageCount();
if (queueCount > 0) {
    Serial.printf("MQTT queue has %u messages waiting\n", queueCount);
}

// Clear queue if needed
if (queueCount > 40) { // Queue is 80% full
    mqttPublisher.clearQueue();
    Serial.println("MQTT queue cleared");
}
```

## Best Practices

### 1. Connection Management

- **Use appropriate timeouts** for your network conditions
- **Monitor connection state** to detect issues early
- **Implement circuit breaker** pattern for critical failures

### 2. Message Handling

- **Enable message queuing** for reliability
- **Set appropriate queue size** based on message volume
- **Monitor queue utilization** to prevent overflow

### 3. Error Handling

- **Log detailed error information** for troubleshooting
- **Implement retry logic** with exponential backoff
- **Handle connection failures gracefully**

### 4. Performance Optimization

- **Batch message processing** to avoid blocking
- **Use appropriate QoS levels** for your use case
- **Monitor publish statistics** for performance tuning

## Troubleshooting

### Common Issues

#### **Connection Timeouts**

- **Symptom**: Frequent connection timeout errors
- **Solution**: Increase `connectionTimeoutMs` to 15000ms or higher
- **Check**: Network latency and broker responsiveness

#### **Message Queue Overflow**

- **Symptom**: Messages being dropped due to full queue
- **Solution**: Increase `maxQueueSize` or improve connection reliability
- **Check**: Network stability and broker availability

#### **Excessive Retries**

- **Symptom**: High retry attempt counts
- **Solution**: Check broker configuration and network stability
- **Check**: Authentication credentials and broker capacity

### Debug Information

The system provides comprehensive debug output:

```
[MQTT] Init host=broker.mqtt.cool port=1883 clientId=relay-1
[MQTT] Connection timeout: 15000ms, Keep alive: 30ms
[MQTT] Message queue initialized with 50 slots
[MQTT] WiFi CONNECTED
[MQTT] Connecting to broker.mqtt.cool:1883 as relay-1...
[MQTT] Connected successfully
[MQTT] Published 4 bytes to farm/tester/remote-3
```

## Performance Metrics

### Expected Performance

- **Connection Time**: < 5 seconds under normal conditions
- **Message Latency**: < 100ms for immediate publishes
- **Queue Processing**: 5 messages per update cycle
- **Memory Usage**: ~12KB for 50-message queue

### Monitoring Recommendations

- **Track connection uptime** percentage
- **Monitor message success rate**
- **Watch queue utilization** trends
- **Log retry attempt patterns**

## Future Enhancements

### Planned Improvements

1. **Persistent Queue**: Store messages in flash memory
2. **Compression**: Compress queued messages to save memory
3. **Priority Queues**: Different priority levels for messages
4. **Health Checks**: Periodic broker health monitoring
5. **Metrics Export**: Export statistics to monitoring systems

### Configuration Validation

- **Parameter validation** in configuration
- **Runtime configuration updates**
- **Configuration backup and restore**

---

This enhanced MQTT implementation provides a robust foundation for reliable farm monitoring communication, with comprehensive error handling, message queuing, and monitoring capabilities.
