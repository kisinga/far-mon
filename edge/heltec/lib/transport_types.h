#pragma once

#include <stdint.h>

// Transport types for identification
enum class TransportType : uint8_t {
    WiFi = 0,
    LoRa = 1,
    USB_Debug = 2,
    Screen = 3,
    I2C_Bus = 4,
    Unknown = 255
};

// Connection states
enum class ConnectionState : uint8_t {
    Disconnected = 0,
    Connecting = 1,
    Connected = 2,
    Error = 3
};

// Transport capabilities
struct TransportCapabilities {
    bool canSend : 1;          // Can send messages
    bool canReceive : 1;       // Can receive messages
    bool supportsAck : 1;      // Supports acknowledgments
    bool supportsBroadcast : 1;// Supports broadcast messages
    bool requiresConnection : 1;// Requires active connection to send
    bool isReliable : 1;       // Reliable delivery
};


