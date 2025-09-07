// TransportInterface - Abstract interface for communication transports
// Defines the contract that all communication channels must implement

#pragma once

#include <cstdint>
#include "message.h"
#include "transport_types.h"

// Forward declaration to avoid circular includes
class CommunicationManager;

// types moved to transport_types.h

// Abstract base class for all transport implementations
class TransportInterface {
public:
    TransportInterface(TransportType type, uint8_t id, CommunicationManager* commMgr = nullptr)
        : transportType(type), transportId(id), commManager(commMgr), state(ConnectionState::Disconnected) {}

    virtual ~TransportInterface() = default;

    // Core transport operations
    virtual bool begin() = 0;                          // Initialize the transport
    virtual void update(uint32_t nowMs) = 0;           // Update transport state
    virtual void end() = 0;                           // Shutdown the transport

    // Message operations
    virtual bool sendMessage(const Message& message) = 0; // Send a message
    virtual bool canSendMessage() const = 0;         // Check if ready to send

    // Connection management
    virtual ConnectionState getConnectionState() const { return state; }
    virtual bool isConnected() const { return state == ConnectionState::Connected; }
    virtual TransportCapabilities getCapabilities() const = 0;

    // Identification
    TransportType getType() const { return transportType; }
    uint8_t getId() const { return transportId; }
    virtual const char* getName() const = 0;

    // Manager registration
    void setCommunicationManager(CommunicationManager* mgr) { commManager = mgr; }
    CommunicationManager* getCommunicationManager() const { return commManager; }

    // Message reception callback (called by concrete implementations)
    virtual void onMessageReceived(const Message& message);

    // Connection state change callback
    virtual void onConnectionStateChanged(ConnectionState newState);

protected:
    TransportType transportType;
    uint8_t transportId;
    CommunicationManager* commManager;
    ConnectionState state;

    // Helper to create messages from raw data (convenience method)
    Message createMessage(Message::Type type, uint8_t dstId, const uint8_t* data, uint16_t len,
                         bool ackRequired = false) {
        return Message(type, transportId, dstId, ackRequired, data, len);
    }
};

// Transport factory function type
using TransportFactory = TransportInterface* (*)();
