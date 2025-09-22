#include "config.h"

#define RELAY_DEVICE_ID 1

RelayConfig buildRelayConfig() {
    RelayConfig cfg = RelayConfig::create(RELAY_DEVICE_ID);
    
    // Override per-device values here (single source of truth for relay)
    cfg.communication.wifi.enableWifi = true;
    cfg.communication.wifi.ssid = "STARLINK";
    cfg.communication.wifi.password = "awesome33";
    // Slow down reconnects to avoid tight loops while associating
    cfg.communication.wifi.reconnectIntervalMs = 15000;
    cfg.communication.wifi.statusCheckIntervalMs = 5000;

    cfg.communication.mqtt.enableMqtt = true;
    cfg.communication.mqtt.brokerHost = "broker.mqtt.cool";
    cfg.communication.mqtt.brokerPort = 1883;
    cfg.communication.mqtt.baseTopic = "farm/tester";
    cfg.communication.mqtt.clientId = "relay-1"; // MQTT client ID should be a string
    // Leave deviceTopic null to publish under baseTopic/<srcId>
    cfg.communication.mqtt.deviceTopic = nullptr;
    
    // Enhanced reliability settings
    cfg.communication.mqtt.connectionTimeoutMs = 15000;    // 15 second timeout
    cfg.communication.mqtt.keepAliveMs = 30;               // 30 second keep alive
    cfg.communication.mqtt.retryIntervalMs = 5000;         // 5 second base retry
    cfg.communication.mqtt.maxRetryIntervalMs = 60000;     // 1 minute max retry
    cfg.communication.mqtt.maxRetryAttempts = 10;          // 10 retry attempts
    cfg.communication.mqtt.maxQueueSize = 50;              // 50 message queue
    cfg.communication.mqtt.enableMessageQueue = true;      // Enable queuing

    cfg.globalDebugMode = true; // Enable debug mode for testing

    return cfg;
}

