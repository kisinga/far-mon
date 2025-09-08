// TransportLoRa - LoRa transport implementation
// Wraps LoRaComm to provide Message-based communication

#pragma once

#include "transport_interface.h"
#include "lora_comm.h"
#include "config.h"

class TransportLoRa : public TransportInterface {
public:
    TransportLoRa(uint8_t id, LoRaComm& loraRef, LoRaComm::Mode mode, uint8_t deviceId, const LoraConfig& loraConfig)
        : TransportInterface(TransportType::LoRa, id), lora(&loraRef), loraMode(mode), deviceId(deviceId), config(loraConfig) {}

    ~TransportLoRa() override {
        end();
    }

    // TransportInterface implementation
    bool begin() override {
        if (lora == nullptr) return false;
        lora->safeBegin(loraMode, deviceId);
        lora->setOnDataReceived(&TransportLoRa::onLoraDataStatic);
        lora->setOnAckReceived(&TransportLoRa::onLoraAckStatic);
        onConnectionStateChanged(ConnectionState::Connected); // LoRa is always "connected"
        return true;
    }

    void update(uint32_t nowMs) override {
        if (lora) lora->tick(nowMs);
    }

    void end() override {
        onConnectionStateChanged(ConnectionState::Disconnected);
    }

    bool sendMessage(const Message& message) override {
        if (!canSendMessage()) return false;

        // Send via LoRa
        if (!lora) return false;
        bool result = lora->sendData(message.getMetadata().destinationId,
                                     message.getPayload(),
                                     message.getLength(),
                                     message.getMetadata().requiresAck);

        if (result) {
            Serial.printf("[LoRa] Sent %d bytes to %d\n",
                         message.getLength(), message.getMetadata().destinationId);
        }

        return result;
    }

    bool canSendMessage() const override {
        return getConnectionState() == ConnectionState::Connected;
    }

    TransportCapabilities getCapabilities() const override {
        return TransportCapabilities{
            .canSend = true,
            .canReceive = true,
            .supportsAck = true,
            .supportsBroadcast = true, // 0xFF destination
            .requiresConnection = false, // LoRa doesn't require "connection"
            .isReliable = false // Best-effort delivery
        };
    }

    const char* getName() const override { return "LoRa"; }

    // LoRa-specific methods
    LoRaComm& getLoraComm() { return *lora; }
    const LoRaComm& getLoraComm() const { return *lora; }

    // Public method to set the instance (for callback setup)
    static void setInstance(TransportLoRa* inst) { instance = inst; }

private:
    LoRaComm* lora;
    LoRaComm::Mode loraMode;
    uint8_t deviceId;
    const LoraConfig& config;

    // Static callbacks for LoRaComm
    static void onLoraDataStatic(uint8_t srcId, const uint8_t* payload, uint8_t length) {
        // Find the transport instance and call its method
        // For now, we'll assume single instance and use a global pointer
        if (instance) {
            instance->onLoraData(srcId, payload, length);
        }
    }

    static void onLoraAckStatic(uint8_t srcId, uint16_t msgId) {
        if (instance) {
            instance->onLoraAck(srcId, msgId);
        }
    }

    void onLoraData(uint8_t srcId, const uint8_t* payload, uint8_t length) {
        if (length > Message::kMaxPayloadSize) {
            length = Message::kMaxPayloadSize;
        }

        // Create message from LoRa data
        Message message(Message::Type::Data, srcId, getId(), false, payload, length);

        // Notify the communication manager
        onMessageReceived(message);
    }

    void onLoraAck(uint8_t srcId, uint16_t msgId) {
        // Handle ACKs if needed
        Serial.printf("[LoRa] ACK from %d for msg %d\n", srcId, msgId);
    }

    // Global instance pointer for callbacks (not ideal, but works for single instance)
    static TransportLoRa* instance;
};

// Initialize static member
TransportLoRa* TransportLoRa::instance = nullptr;
