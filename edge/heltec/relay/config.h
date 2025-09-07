#pragma once

#include "../lib/config.h"

// Per-device configuration for Relay
static constexpr const char* RELAY_DEVICE_ID = "01";

inline RelayConfig buildRelayConfig() {
    RelayConfig cfg = RelayConfig::create(RELAY_DEVICE_ID);
    // Override per-device values here (single source of truth for relay)
    cfg.communication.wifi.enableWifi = true;
    cfg.communication.wifi.ssid = "STARLINK";
    cfg.communication.wifi.password = "awesome33";

    cfg.communication.mqtt.enableMqtt = true;
    cfg.communication.mqtt.brokerHost = "192.168.1.180";
    cfg.communication.mqtt.brokerPort = 1883;
    cfg.communication.mqtt.baseTopic = "farm/telemetry";
    cfg.communication.mqtt.clientId = RELAY_DEVICE_ID;
    cfg.communication.mqtt.deviceTopic = RELAY_DEVICE_ID;
    return cfg;
}


