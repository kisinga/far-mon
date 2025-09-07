// CommunicationManager Implementation

#include "communication_manager.h"
#include "transport_interface.h"
#include <algorithm>
#include <Arduino.h>

CommunicationManager::~CommunicationManager() {
    cleanupTransports();
}

bool CommunicationManager::registerTransport(TransportInterface* transport) {
    if (!transport) return false;

    // Check if already registered
    if (findTransport(transport->getType(), transport->getId())) {
        return false;
    }

    transport->setCommunicationManager(this);
    transports.push_back(transport);
    return true;
}

bool CommunicationManager::unregisterTransport(TransportType type, uint8_t id) {
    auto it = std::find_if(transports.begin(), transports.end(),
                          [type, id](TransportInterface* t) {
                              return t->getType() == type && t->getId() == id;
                          });

    if (it != transports.end()) {
        (*it)->setCommunicationManager(nullptr);
        transports.erase(it);
        return true;
    }
    return false;
}

TransportInterface* CommunicationManager::getTransport(TransportType type, uint8_t id) const {
    return findTransport(type, id);
}

std::vector<TransportInterface*> CommunicationManager::getTransportsByType(TransportType type) const {
    std::vector<TransportInterface*> result;
    for (auto transport : transports) {
        if (transport->getType() == type) {
            result.push_back(transport);
        }
    }
    return result;
}

void CommunicationManager::addRoutingRule(const RoutingRule& rule) {
    routingRules.push_back(rule);
}

void CommunicationManager::removeRoutingRule(size_t index) {
    if (index < routingRules.size()) {
        routingRules.erase(routingRules.begin() + index);
    }
}

void CommunicationManager::routeMessage(const Message& message, TransportInterface* sourceTransport) {
    TransportType sourceType = sourceTransport ? sourceTransport->getType() : TransportType::Unknown;

    // Find all matching routing rules and attempt to route
    for (const auto& rule : routingRules) {
        if (!rule.enabled) continue;

        // Check if rule applies to this message
        if (!shouldRouteMessage(message, sourceType, rule.destinationType)) continue;

        // Find destination transports
        auto destTransports = getTransportsByType(rule.destinationType);
        for (auto destTransport : destTransports) {
            if (!destTransport->canSendMessage()) continue;
            if (destTransport->getConnectionState() != ConnectionState::Connected &&
                destTransport->getCapabilities().requiresConnection) {
                continue;
            }

            // Create a copy of the message with updated routing info
            Message routedMessage = message;
            routedMessage.setDestinationId(destTransport->getId());

            // Attempt to send
            if (destTransport->sendMessage(routedMessage)) {
                stats.messagesRouted++;
                Serial.printf("[CommMgr] Routed %s from %s to %s\n",
                            message.getType() == Message::Type::Data ? "DATA" : "MSG",
                            sourceTransport->getName(), destTransport->getName());
            } else {
                stats.messagesDropped++;
                Serial.printf("[CommMgr] Failed to route to %s\n", destTransport->getName());
            }
        }
    }
}

void CommunicationManager::onTransportStateChanged(TransportInterface* transport, ConnectionState newState) {
    if (!transport) return;

    stats.transportStateChanges++;
    Serial.printf("[CommMgr] %s state: %s\n", transport->getName(),
                newState == ConnectionState::Connected ? "CONNECTED" :
                newState == ConnectionState::Disconnected ? "DISCONNECTED" :
                newState == ConnectionState::Connecting ? "CONNECTING" : "ERROR");
}

void CommunicationManager::update(uint32_t nowMs) {
    for (auto transport : transports) {
        if (transport) {
            transport->update(nowMs);
        }
    }
}

void CommunicationManager::printStatus() const {
    Serial.println("[CommMgr] Transport Status:");
    for (auto transport : transports) {
        if (transport) {
            const char* stateStr = "UNKNOWN";
            switch (transport->getConnectionState()) {
                case ConnectionState::Connected: stateStr = "CONNECTED"; break;
                case ConnectionState::Disconnected: stateStr = "DISCONNECTED"; break;
                case ConnectionState::Connecting: stateStr = "CONNECTING"; break;
                case ConnectionState::Error: stateStr = "ERROR"; break;
            }
            Serial.printf("  %s: %s\n", transport->getName(), stateStr);
        }
    }
    Serial.printf("[CommMgr] Stats: %lu routed, %lu dropped, %lu state changes\n",
                (unsigned long)stats.messagesRouted, (unsigned long)stats.messagesDropped,
                (unsigned long)stats.transportStateChanges);
}

void CommunicationManager::enableAllRoutes() {
    for (auto& rule : routingRules) {
        rule.enabled = true;
    }
}

void CommunicationManager::disableAllRoutes() {
    for (auto& rule : routingRules) {
        rule.enabled = false;
    }
}

void CommunicationManager::enableRoute(TransportType srcType, uint8_t srcId, TransportType dstType, uint8_t dstId) {
    // Find and enable matching rule
    for (auto& rule : routingRules) {
        if (rule.sourceType == srcType && rule.destinationType == dstType) {
            rule.enabled = true;
            return;
        }
    }
    // If no rule exists, add one
    addRoutingRule(RoutingRule(Message::Type::Data, srcType, dstType, false, true));
}

void CommunicationManager::disableRoute(TransportType srcType, uint8_t srcId, TransportType dstType, uint8_t dstId) {
    for (auto& rule : routingRules) {
        if (rule.sourceType == srcType && rule.destinationType == dstType) {
            rule.enabled = false;
            return;
        }
    }
}

bool CommunicationManager::shouldRouteMessage(const Message& message, TransportType sourceType, TransportType destType) const {
    // Check if source type matches
    if (sourceType != TransportType::Unknown && sourceType != destType) {
        // This is a destination rule, source should match the message's source
        return false;
    }

    // Could add more sophisticated filtering here (by message type, content, etc.)
    return true;
}

TransportInterface* CommunicationManager::findTransport(TransportType type, uint8_t id) const {
    for (auto transport : transports) {
        if (transport && transport->getType() == type && transport->getId() == id) {
            return transport;
        }
    }
    return nullptr;
}

void CommunicationManager::cleanupTransports() {
    for (auto transport : transports) {
        if (transport) {
            transport->setCommunicationManager(nullptr);
        }
    }
    transports.clear();
}
