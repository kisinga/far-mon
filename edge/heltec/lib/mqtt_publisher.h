// MQTT Publisher - Minimal wrapper with optional PubSubClient integration
// If PubSubClient is not available, falls back to Serial logging.

#pragma once

#include <Arduino.h>
#include <WiFi.h>

// Enable PubSubClient usage by defining ENABLE_PUBSUBCLIENT at build time
#ifdef ENABLE_PUBSUBCLIENT
#include <PubSubClient.h>
#endif

struct MqttPublisherConfig {
    bool enableMqtt = false;
    const char* brokerHost = "192.168.1.180";
    uint16_t brokerPort = 1883;
    const char* clientId = "device";
    const char* username = nullptr;
    const char* password = nullptr;
    const char* baseTopic = "farm/telemetry";
    const char* deviceTopic = nullptr; // optional suffix override
    uint8_t qos = 0;
    bool retain = false;
};

class MqttPublisher {
public:
    explicit MqttPublisher(const MqttPublisherConfig& config)
        : cfg(config) {}

    void begin() {
        if (!cfg.enableMqtt) {
            Serial.println(F("[MQTT] Disabled by config; skipping init"));
            return;
        }
        Serial.print(F("[MQTT] Init "));
        Serial.print(F("host=")); Serial.print(cfg.brokerHost ? cfg.brokerHost : "(null)");
        Serial.print(F(" port=")); Serial.print((unsigned)cfg.brokerPort);
        Serial.print(F(" clientId=")); Serial.print(cfg.clientId ? cfg.clientId : "(null)");
        Serial.print(F(" baseTopic=")); Serial.print(cfg.baseTopic ? cfg.baseTopic : "(null)");
        Serial.print(F(" deviceTopic=")); Serial.print((cfg.deviceTopic && cfg.deviceTopic[0] != '\0') ? cfg.deviceTopic : "(auto)");
        Serial.print(F(" qos=")); Serial.print((unsigned)cfg.qos);
        Serial.print(F(" retain=")); Serial.println(cfg.retain ? F("true") : F("false"));
#ifdef ENABLE_PUBSUBCLIENT
        wifiClient = std::make_unique<WiFiClient>();
        client = std::make_unique<PubSubClient>(*wifiClient);
        client->setServer(cfg.brokerHost, cfg.brokerPort);
#endif
        lastConnAttemptMs = 0;
    }

    void update(uint32_t nowMs) {
        if (!cfg.enableMqtt) return;
        bool wifiUp = (WiFi.status() == WL_CONNECTED);
        if (wifiUp != lastWifiConnected) {
            Serial.printf("[MQTT] WiFi %s\n", wifiUp ? "CONNECTED" : "DISCONNECTED");
            lastWifiConnected = wifiUp;
        }
        if (!wifiUp) return;
#ifdef ENABLE_PUBSUBCLIENT
        if (client) {
            bool mqttUp = client->connected();
            if (mqttUp != lastMqttConnected) {
                Serial.printf("[MQTT] %s\n", mqttUp ? "SESSION CONNECTED" : "SESSION DISCONNECTED");
                lastMqttConnected = mqttUp;
            }
            if (!mqttUp) {
                if ((int32_t)(nowMs - lastConnAttemptMs) >= 0) {
                    reconnect();
                    lastConnAttemptMs = nowMs + 3000; // retry every 3s
                }
                return;
            }
            client->loop();
        }
#endif
    }

    bool isReady() const {
        if (!cfg.enableMqtt) return false;
        if (WiFi.status() != WL_CONNECTED) return false;
#ifdef ENABLE_PUBSUBCLIENT
        return client && client->connected();
#else
        return false;
#endif
    }

    // Publish using baseTopic + "/" + topicSuffix
    bool publish(const char* topicSuffix, const uint8_t* payload, uint8_t length) {
        if (!cfg.enableMqtt) return false;
        if (!payload || length == 0) return false;
        char topic[128];
        if (cfg.deviceTopic && cfg.deviceTopic[0] != '\0') {
            snprintf(topic, sizeof(topic), "%s/%s", cfg.baseTopic ? cfg.baseTopic : "farm/telemetry", cfg.deviceTopic);
        } else if (topicSuffix && topicSuffix[0] != '\0') {
            snprintf(topic, sizeof(topic), "%s/%s", cfg.baseTopic ? cfg.baseTopic : "farm/telemetry", topicSuffix);
        } else {
            snprintf(topic, sizeof(topic), "%s", cfg.baseTopic ? cfg.baseTopic : "farm/telemetry");
        }

#ifdef ENABLE_PUBSUBCLIENT
        if (client && client->connected()) {
            bool ok = client->publish(topic, payload, length, cfg.retain);
            if (!ok) {
                Serial.printf("[MQTT] Publish failed to %s\n", topic);
            } else {
                Serial.printf("[MQTT] Published %u bytes to %s\n", (unsigned)length, topic);
            }
            return ok;
        }
#endif
        // Fallback: log
        Serial.print(F("[MQTT] (stub) topic="));
        Serial.print(topic);
        Serial.print(F(" payload="));
        for (uint8_t i = 0; i < length; i++) Serial.write(payload[i]);
        Serial.println();
        return true;
    }

private:
    MqttPublisherConfig cfg;
    uint32_t lastConnAttemptMs = 0;
    bool lastWifiConnected = false;
    bool lastMqttConnected = false;
#ifdef ENABLE_PUBSUBCLIENT
    std::unique_ptr<WiFiClient> wifiClient;
    std::unique_ptr<PubSubClient> client;

    void reconnect() {
        if (!client) return;
        Serial.printf("[MQTT] Connecting to %s:%u as %s...\n", cfg.brokerHost, (unsigned)cfg.brokerPort, cfg.clientId ? cfg.clientId : "device");
        bool ok;
        if (cfg.username && cfg.password) {
            ok = client->connect(cfg.clientId ? cfg.clientId : "device", cfg.username, cfg.password);
        } else {
            ok = client->connect(cfg.clientId ? cfg.clientId : "device");
        }
        Serial.println(ok ? F("[MQTT] Connected") : F("[MQTT] Connect failed"));
    }
#endif
};


