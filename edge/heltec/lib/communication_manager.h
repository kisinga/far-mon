// CommunicationManager - Central routing hub for all communication channels
// Manages transport registration, routing configuration, and message forwarding

#pragma once

#include <vector>
#include <functional>
#include <memory>
#include <algorithm>
#include <Arduino.h>
#include "message.h"
#include "transport_interface.h"

// Use shared transport types
#include "transport_types.h"

// Forward declare TransportInterface and CommunicationManager to avoid circulars
class TransportInterface;
class CommunicationManager;

// ============================================================================
// SOLID REFACTORED COMMUNICATION SYSTEM (declarations only)
// ============================================================================

// TransportRegistry - Single Responsibility: Transport registration and lookup
class TransportRegistry {
public:
    bool registerTransport(TransportInterface* transport);
    bool unregisterTransport(TransportType type, uint8_t id);
    TransportInterface* getTransport(TransportType type, uint8_t id) const;
    std::vector<TransportInterface*> getTransportsByType(TransportType type) const;
    size_t getTransportCount() const { return transports.size(); }
    void cleanupTransports();

private:
    std::vector<TransportInterface*> transports;
    TransportInterface* findTransport(TransportType type, uint8_t id) const;
};

// MessageRouter - Single Responsibility: Message routing logic
class MessageRouter {
public:
    // Routing rule: defines when and how to route messages
    struct RoutingRule {
        Message::Type messageType;        // Type of message to route (or wildcard)
        TransportType sourceType;         // Source transport type
        TransportType destinationType;    // Destination transport type
        bool requiresAck;                 // Whether to require acknowledgment
        bool enabled;                     // Whether this rule is active

        RoutingRule(Message::Type msgType = Message::Type::Data,
                   TransportType srcType = TransportType::Unknown,
                   TransportType dstType = TransportType::Unknown,
                   bool ack = false, bool enable = true)
            : messageType(msgType), sourceType(srcType), destinationType(dstType),
              requiresAck(ack), enabled(enable) {}
    };

    void addRoutingRule(const RoutingRule& rule);
    void removeRoutingRule(size_t index);
    const std::vector<RoutingRule>& getRoutingRules() const { return routingRules; }

    // Configuration helpers
    void enableAllRoutes();
    void disableAllRoutes();
    void enableRoute(TransportType srcType, TransportType dstType);
    void disableRoute(TransportType srcType, TransportType dstType);

    // Route a message using registered transports
    bool routeMessage(const Message& message, TransportInterface* sourceTransport,
                     const TransportRegistry& registry);

private:
    std::vector<RoutingRule> routingRules;
    bool shouldRouteMessage(const Message& message, TransportType sourceType, TransportType destType) const;
};

// CommunicationStats - Single Responsibility: Statistics collection and reporting
class CommunicationStats {
public:
    void recordMessageRouted();
    void recordMessageDropped();
    void recordTransportStateChange();
    void printStatus(const TransportRegistry& registry) const;

private:
    struct Stats {
        uint32_t messagesRouted = 0;
        uint32_t messagesDropped = 0;
        uint32_t transportStateChanges = 0;
    } stats;
};

// CommunicationManager - Facade: Orchestrates the communication system (SOLID)
class CommunicationManager {
public:
    CommunicationManager();
    ~CommunicationManager();

    // Transport management (delegates to TransportRegistry)
    bool registerTransport(TransportInterface* transport);
    bool unregisterTransport(TransportType type, uint8_t id);
    TransportInterface* getTransport(TransportType type, uint8_t id) const;
    std::vector<TransportInterface*> getTransportsByType(TransportType type) const;
    size_t getTransportCount() const;

    // Routing configuration (delegates to MessageRouter)
    void addRoutingRule(const MessageRouter::RoutingRule& rule);
    void removeRoutingRule(size_t index);
    const std::vector<MessageRouter::RoutingRule>& getRoutingRules() const;

    // Message routing (called by transports)
    void routeMessage(const Message& message, TransportInterface* sourceTransport);

    // Transport state change notification
    void onTransportStateChanged(TransportInterface* transport, ConnectionState newState);

    // Utility methods
    void update(uint32_t nowMs); // Update all transports
    void printStatus() const; // Debug: print all transport states

    // Configuration helpers
    void enableAllRoutes();
    void disableAllRoutes();
    void enableRoute(TransportType srcType, TransportType dstType);
    void disableRoute(TransportType srcType, TransportType dstType);

private:
    TransportRegistry registry;
    MessageRouter router;
    CommunicationStats stats;
};

// ============================================================================
// SOLID IMPLEMENTATIONS (moved to .cpp in future; kept inline for Arduino)
// ============================================================================

// TransportRegistry implementations
inline bool TransportRegistry::registerTransport(TransportInterface* transport) {
    if (!transport) return false;
    if (findTransport(transport->getType(), transport->getId())) return false;

    transports.push_back(transport);
    return true;
}

inline bool TransportRegistry::unregisterTransport(TransportType type, uint8_t id) {
    auto it = std::find_if(transports.begin(), transports.end(),
                          [type, id](TransportInterface* t) {
                              return t->getType() == type && t->getId() == id;
                          });
    if (it != transports.end()) {
        transports.erase(it);
        return true;
    }
    return false;
}

inline TransportInterface* TransportRegistry::getTransport(TransportType type, uint8_t id) const {
    return findTransport(type, id);
}

inline std::vector<TransportInterface*> TransportRegistry::getTransportsByType(TransportType type) const {
    std::vector<TransportInterface*> result;
    for (auto transport : transports) {
        if (transport->getType() == type) {
            result.push_back(transport);
        }
    }
    return result;
}

inline void TransportRegistry::cleanupTransports() {
    transports.clear();
}

inline TransportInterface* TransportRegistry::findTransport(TransportType type, uint8_t id) const {
    for (auto transport : transports) {
        if (transport && transport->getType() == type && transport->getId() == id) {
            return transport;
        }
    }
    return nullptr;
}

// MessageRouter implementations
inline void MessageRouter::addRoutingRule(const RoutingRule& rule) {
    routingRules.push_back(rule);
}

inline void MessageRouter::removeRoutingRule(size_t index) {
    if (index < routingRules.size()) {
        routingRules.erase(routingRules.begin() + index);
    }
}

inline void MessageRouter::enableAllRoutes() {
    for (auto& rule : routingRules) {
        rule.enabled = true;
    }
}

inline void MessageRouter::disableAllRoutes() {
    for (auto& rule : routingRules) {
        rule.enabled = false;
    }
}

inline void MessageRouter::enableRoute(TransportType srcType, TransportType dstType) {
    for (auto& rule : routingRules) {
        if (rule.sourceType == srcType && rule.destinationType == dstType) {
            rule.enabled = true;
            return;
        }
    }
    addRoutingRule(RoutingRule(Message::Type::Data, srcType, dstType, false, true));
}

inline void MessageRouter::disableRoute(TransportType srcType, TransportType dstType) {
    for (auto& rule : routingRules) {
        if (rule.sourceType == srcType && rule.destinationType == dstType) {
            rule.enabled = false;
            return;
        }
    }
}

inline bool MessageRouter::routeMessage(const Message& message, TransportInterface* sourceTransport,
                                const TransportRegistry& registry) {
    if (!sourceTransport) return false;

    TransportType sourceType = sourceTransport->getType();
    bool messageRouted = false;

    for (const auto& rule : routingRules) {
        if (!rule.enabled) continue;
        if (!shouldRouteMessage(message, sourceType, rule.destinationType)) continue;

        auto destTransports = registry.getTransportsByType(rule.destinationType);
        for (auto destTransport : destTransports) {
            if (!destTransport->canSendMessage()) continue;
            if (destTransport->getConnectionState() != ConnectionState::Connected &&
                destTransport->getCapabilities().requiresConnection) {
                continue;
            }

            Message routedMessage = message;
            routedMessage.setDestinationId(destTransport->getId());

            if (destTransport->sendMessage(routedMessage)) {
                Serial.printf("[Router] Routed %s from %s to %s\n",
                            message.getType() == Message::Type::Data ? "DATA" : "MSG",
                            sourceTransport->getName(), destTransport->getName());
                messageRouted = true;
            } else {
                Serial.printf("[Router] Failed to route to %s\n", destTransport->getName());
            }
        }
    }
    return messageRouted;
}

inline bool MessageRouter::shouldRouteMessage(const Message& message, TransportType sourceType, TransportType destType) const {
    if (sourceType != TransportType::Unknown && sourceType != destType) {
        return false;
    }
    return true;
}

// CommunicationStats implementations
inline void CommunicationStats::recordMessageRouted() {
    stats.messagesRouted++;
}

inline void CommunicationStats::recordMessageDropped() {
    stats.messagesDropped++;
}

inline void CommunicationStats::recordTransportStateChange() {
    stats.transportStateChanges++;
}

inline void CommunicationStats::printStatus(const TransportRegistry& registry) const {
    Serial.println("[Stats] Transport Status:");

    // Print status for all transport types
    static const TransportType allTypes[] = {
        TransportType::WiFi, TransportType::LoRa, TransportType::USB_Debug,
        TransportType::Screen, TransportType::I2C_Bus
    };

    for (auto type : allTypes) {
        auto transports = registry.getTransportsByType(type);
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
    }

    Serial.printf("[Stats] %lu routed, %lu dropped, %lu state changes\n",
                (unsigned long)stats.messagesRouted, (unsigned long)stats.messagesDropped,
                (unsigned long)stats.transportStateChanges);
}

// CommunicationManager implementations (Facade pattern)
inline CommunicationManager::CommunicationManager() = default;

inline CommunicationManager::~CommunicationManager() {
    registry.cleanupTransports();
}

inline bool CommunicationManager::registerTransport(TransportInterface* transport) {
    if (!transport) return false;
    transport->setCommunicationManager(this);
    return registry.registerTransport(transport);
}

inline bool CommunicationManager::unregisterTransport(TransportType type, uint8_t id) {
    return registry.unregisterTransport(type, id);
}

inline TransportInterface* CommunicationManager::getTransport(TransportType type, uint8_t id) const {
    return registry.getTransport(type, id);
}

inline std::vector<TransportInterface*> CommunicationManager::getTransportsByType(TransportType type) const {
    return registry.getTransportsByType(type);
}

inline size_t CommunicationManager::getTransportCount() const {
    return registry.getTransportCount();
}

inline void CommunicationManager::addRoutingRule(const MessageRouter::RoutingRule& rule) {
    router.addRoutingRule(rule);
}

inline void CommunicationManager::removeRoutingRule(size_t index) {
    router.removeRoutingRule(index);
}

inline const std::vector<MessageRouter::RoutingRule>& CommunicationManager::getRoutingRules() const {
    return router.getRoutingRules();
}

inline void CommunicationManager::routeMessage(const Message& message, TransportInterface* sourceTransport) {
    if (router.routeMessage(message, sourceTransport, registry)) {
        stats.recordMessageRouted();
    } else {
        stats.recordMessageDropped();
    }
}

inline void CommunicationManager::onTransportStateChanged(TransportInterface* transport, ConnectionState newState) {
    stats.recordTransportStateChange();
    Serial.printf("[CommMgr] %s state: %s\n", transport->getName(),
                newState == ConnectionState::Connected ? "CONNECTED" :
                newState == ConnectionState::Disconnected ? "DISCONNECTED" :
                newState == ConnectionState::Connecting ? "CONNECTING" : "ERROR");
}

inline void CommunicationManager::update(uint32_t nowMs) {
    // Update all registered transports of all types
    static const TransportType allTypes[] = {
        TransportType::WiFi, TransportType::LoRa, TransportType::USB_Debug,
        TransportType::Screen, TransportType::I2C_Bus
    };

    for (auto type : allTypes) {
        auto transports = registry.getTransportsByType(type);
        for (auto transport : transports) {
            if (transport) {
                transport->update(nowMs);
            }
        }
    }
}

inline void CommunicationManager::printStatus() const {
    stats.printStatus(registry);
}

inline void CommunicationManager::enableAllRoutes() {
    router.enableAllRoutes();
}

inline void CommunicationManager::disableAllRoutes() {
    router.disableAllRoutes();
}

inline void CommunicationManager::enableRoute(TransportType srcType, TransportType dstType) {
    router.enableRoute(srcType, dstType);
}

inline void CommunicationManager::disableRoute(TransportType srcType, TransportType dstType) {
    router.disableRoute(srcType, dstType);
}
