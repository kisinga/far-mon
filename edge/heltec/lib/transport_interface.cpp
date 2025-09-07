#include "transport_interface.h"
#include "communication_manager.h"

void TransportInterface::onMessageReceived(const Message& message) {
    if (commManager) {
        commManager->routeMessage(message, this);
    }
}

void TransportInterface::onConnectionStateChanged(ConnectionState newState) {
    state = newState;
    if (commManager) {
        commManager->onTransportStateChanged(this, newState);
    }
}


