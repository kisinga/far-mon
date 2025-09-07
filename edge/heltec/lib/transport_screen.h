// TransportScreen - Screen/OLED display transport implementation
// Shows messages on the OLED display

#pragma once

#include "transport_interface.h"
#include "display_provider.h"
#include "communication_config.h"

class TransportScreen : public TransportInterface {
public:
    TransportScreen(uint8_t id, OledDisplay* display, const ScreenConfig& screenConfig)
        : TransportInterface(TransportType::Screen, id), oledDisplay(display), config(screenConfig) {}

    ~TransportScreen() override {
        end();
    }

    // TransportInterface implementation
    bool begin() override {
        if (config.enableScreen && oledDisplay) {
            onConnectionStateChanged(ConnectionState::Connected);
            return true;
        }
        return false;
    }

    void update(uint32_t nowMs) override {
        // Screen is always "connected" if display is available and enabled
        ConnectionState newState = (config.enableScreen && oledDisplay) ?
                                  ConnectionState::Connected : ConnectionState::Disconnected;
        if (newState != getConnectionState()) {
            onConnectionStateChanged(newState);
        }
    }

    void end() override {
        onConnectionStateChanged(ConnectionState::Disconnected);
    }

    bool sendMessage(const Message& message) override {
        if (!canSendMessage() || !oledDisplay) return false;

        // Display message on screen
        char buffer[32];
        const char* typeStr = getMessageTypeString(message.getType());

        if (config.enableTimestamp) {
            snprintf(buffer, sizeof(buffer), "[%lu] %s: %d bytes", millis(), typeStr, message.getLength());
        } else {
            snprintf(buffer, sizeof(buffer), "%s: %d bytes", typeStr, message.getLength());
        }

        // Use the display's notification system if available
        // For now, just print to serial as placeholder
        Serial.printf("[Screen] Displaying: %s\n", buffer);

        return true;
    }

    bool canSendMessage() const override {
        return config.enableScreen && getConnectionState() == ConnectionState::Connected && oledDisplay != nullptr;
    }

    TransportCapabilities getCapabilities() const override {
        return TransportCapabilities{
            .canSend = true,
            .canReceive = false, // Screen is outbound only
            .supportsAck = false,
            .supportsBroadcast = false,
            .requiresConnection = true,
            .isReliable = true // Display is reliable
        };
    }

    const char* getName() const override { return "Screen"; }

private:
    OledDisplay* oledDisplay;
    const ScreenConfig& config;

    const char* getMessageTypeString(Message::Type type) const {
        switch (type) {
            case Message::Type::Data: return "DATA";
            case Message::Type::Command: return "CMD";
            case Message::Type::Status: return "STATUS";
            case Message::Type::Debug: return "DEBUG";
            case Message::Type::Telemetry: return "TEL";
            case Message::Type::Heartbeat: return "HB";
            default: return "UNK";
        }
    }
};
