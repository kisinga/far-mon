#pragma once

#include "lib/config.h"

// Per-device configuration for Relay
static constexpr const char* RELAY_DEVICE_ID = "01";

inline RelayConfig buildRelayConfig() {
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
    cfg.communication.mqtt.clientId = RELAY_DEVICE_ID;
    // Leave deviceTopic null to publish under baseTopic/<srcId>
    cfg.communication.mqtt.deviceTopic = nullptr;

    // Enable communication manager for routing functionality
    cfg.communication.enableCommunicationManager = true;

    return cfg;
}


