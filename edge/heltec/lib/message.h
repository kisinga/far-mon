// Message - Generic message structure for inter-transport communication
// Provides a unified interface for data exchange between different communication channels

#pragma once

#include <cstdint>
#include <cstring>
#include <Arduino.h>

class Message {
public:
    // Message types to categorize different kinds of data
    enum class Type : uint8_t {
        Data = 0,       // Application data
        Command = 1,    // Control commands
        Status = 2,     // Status information
        Debug = 3,      // Debug information
        Telemetry = 4,  // Sensor/telemetry data
        Heartbeat = 5   // Keep-alive messages
    };

    // Metadata for routing and processing
    struct Metadata {
        uint32_t timestamp;      // When message was created
        uint8_t sourceId;        // Source identifier
        uint8_t destinationId;   // Destination identifier (0xFF for broadcast)
        Type type;              // Message type
        uint16_t sequenceId;     // Sequence number for ordering
        bool requiresAck;       // Whether acknowledgment is required
    };

    // Constructor for creating messages
    Message(Type msgType = Type::Data, uint8_t srcId = 0, uint8_t dstId = 0xFF,
            bool ackRequired = false, const uint8_t* data = nullptr, uint16_t dataLen = 0)
        : length(dataLen) {

        metadata.timestamp = millis();
        metadata.sourceId = srcId;
        metadata.destinationId = dstId;
        metadata.type = msgType;
        metadata.sequenceId = nextSequenceId++;
        metadata.requiresAck = ackRequired;

        if (data && dataLen > 0 && dataLen <= kMaxPayloadSize) {
            memcpy(payload, data, dataLen);
            length = dataLen;
        }
    }

    // Accessors
    const Metadata& getMetadata() const { return metadata; }
    const uint8_t* getPayload() const { return payload; }
    uint16_t getLength() const { return length; }
    Type getType() const { return metadata.type; }

    // Modifiers
    void setSourceId(uint8_t id) { metadata.sourceId = id; }
    void setDestinationId(uint8_t id) { metadata.destinationId = id; }
    void setType(Type type) { metadata.type = type; }
    void setRequiresAck(bool ack) { metadata.requiresAck = ack; }

    // Utility methods
    bool isBroadcast() const { return metadata.destinationId == 0xFF; }
    bool isEmpty() const { return length == 0; }

    // Static constants
    static constexpr uint16_t kMaxPayloadSize = 64;
    static constexpr uint16_t kTotalSize = sizeof(Metadata) + kMaxPayloadSize;

private:
    Metadata metadata;
    uint8_t payload[kMaxPayloadSize];
    uint16_t length;

    static inline uint16_t nextSequenceId = 1; // Global sequence counter
};

