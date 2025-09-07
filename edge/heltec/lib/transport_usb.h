// TransportUSB - USB/Serial debug transport implementation
// Sends messages to serial console for debugging

#pragma once

#include "transport_interface.h"
#include "config.h"

class TransportUSB : public TransportInterface {
public:
    TransportUSB(uint8_t id, const UsbConfig& usbConfig)
        : TransportInterface(TransportType::USB_Debug, id), config(usbConfig) {
        // Initialize serial if not already done
        if (!Serial && config.enableDebug) {
            Serial.begin(config.baudRate);
        }
    }

    ~TransportUSB() override {
        end();
    }

    // TransportInterface implementation
    bool begin() override {
        if (config.enableDebug && !Serial) {
            Serial.begin(config.baudRate);
        }
        onConnectionStateChanged(ConnectionState::Connected);
        return true;
    }

    void update(uint32_t nowMs) override {
        // USB/Serial is always "connected" if Serial is available and enabled
        ConnectionState newState = (config.enableDebug && Serial) ?
                                  ConnectionState::Connected : ConnectionState::Disconnected;
        if (newState != getConnectionState()) {
            onConnectionStateChanged(newState);
        }
    }

    void end() override {
        onConnectionStateChanged(ConnectionState::Disconnected);
    }

    bool sendMessage(const Message& message) override {
        if (!canSendMessage()) return false;

        // Format and print message to serial
        if (config.enableTimestamp) {
            Serial.printf(F("[USB %lu] "), millis());
        } else {
            Serial.print(F("[USB] "));
        }

        // Print message type
        switch (message.getType()) {
            case Message::Type::Data: Serial.print(F("DATA")); break;
            case Message::Type::Command: Serial.print(F("CMD")); break;
            case Message::Type::Status: Serial.print(F("STATUS")); break;
            case Message::Type::Debug: Serial.print(F("DEBUG")); break;
            case Message::Type::Telemetry: Serial.print(F("TELEMETRY")); break;
            case Message::Type::Heartbeat: Serial.print(F("HEARTBEAT")); break;
            default: Serial.print(F("UNKNOWN")); break;
        }

        Serial.printf(F(" from=%d to=%d len=%d: "),
                     message.getMetadata().sourceId,
                     message.getMetadata().destinationId,
                     message.getLength());

        // Print payload as hex
        const uint8_t* payload = message.getPayload();
        for (uint16_t i = 0; i < message.getLength(); i++) {
            if (payload[i] >= 32 && payload[i] <= 126) {
                Serial.write(payload[i]); // Printable ASCII
            } else {
                Serial.printf(F("\\x%02X"), payload[i]); // Hex for non-printable
            }
        }
        Serial.println();

        return true;
    }

    bool canSendMessage() const override {
        return config.enableDebug && getConnectionState() == ConnectionState::Connected && Serial;
    }

    TransportCapabilities getCapabilities() const override {
        return TransportCapabilities{
            .canSend = true,
            .canReceive = false, // USB is outbound only in this design
            .supportsAck = false,
            .supportsBroadcast = false,
            .requiresConnection = true,
            .isReliable = true // Serial is reliable
        };
    }

    const char* getName() const override { return "USB"; }

private:
    const UsbConfig& config;
};
