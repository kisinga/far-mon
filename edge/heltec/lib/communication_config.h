// Communication Configuration - Centralized configuration for all communication channels
// Consolidates USB, LoRa, WiFi, and routing configurations

#pragma once

#include <cstdint>
#include "wifi_manager.h"
#include "message.h"
// Use shared transport types
#include "transport_types.h"

// USB Configuration
struct UsbConfig {
    bool enableDebug = true;           // Enable USB debug output
    uint32_t baudRate = 115200;        // Serial baud rate
    bool enableTimestamp = true;       // Include timestamps in debug output
    bool enableColorOutput = false;    // Enable ANSI color codes (if supported)
    uint8_t debugLevel = 3;            // Default debug level (0-5, higher = more verbose)

    // Advanced settings
    uint16_t rxBufferSize = 256;       // Receive buffer size
    uint16_t txBufferSize = 256;       // Transmit buffer size
    bool enableFlowControl = false;    // Hardware flow control (RTS/CTS)
};

// LoRa Configuration
struct LoraConfig {
    bool enableLora = true;            // Enable LoRa communication
    uint32_t frequency = 868000000UL;  // Operating frequency (Hz)
    uint8_t txPower = 14;              // Transmit power (dBm)
    uint8_t spreadingFactor = 7;       // Spreading factor (6-12)
    uint8_t codingRate = 1;            // Coding rate (1=4/5, 2=4/6, 3=4/7, 4=4/8)
    uint8_t bandwidth = 0;             // Bandwidth (0=125kHz, 1=250kHz, 2=500kHz)
    uint8_t preambleLength = 8;        // Preamble length
    uint32_t symbolTimeout = 0;        // Symbol timeout
    bool iqInvert = false;             // IQ invert for RX
    uint8_t maxPayload = 64;           // Maximum payload size
    uint8_t maxOutbox = 8;             // Maximum outgoing message queue
    uint8_t maxPeers = 16;             // Maximum peer count (master mode)
    uint32_t ackTimeoutMs = 1500;      // ACK timeout
    uint8_t maxRetries = 4;            // Maximum retry attempts
    uint32_t pingIntervalMs = 30000;   // Ping interval (slave mode)

    // Advanced timing
    uint32_t masterTtlMs = 15000;      // Master TTL for peer tracking
};

// WiFi Configuration
struct WifiCommConfig {
    bool enableWifi = true;            // Enable WiFi communication
    const char* ssid = "YourWiFiNetwork";     // WiFi network name
    const char* password = "YourWiFiPassword"; // WiFi password
    uint32_t reconnectIntervalMs = 30000;    // Reconnect interval
    uint32_t statusCheckIntervalMs = 5000;   // Status check interval

    // Connection settings
    uint8_t maxReconnectAttempts = 10; // Maximum reconnect attempts
    bool enableDhcp = true;            // Enable DHCP
    const char* staticIp = nullptr;    // Static IP (if DHCP disabled)
    const char* subnetMask = nullptr;  // Subnet mask
    const char* gateway = nullptr;     // Gateway IP
    const char* dns = nullptr;         // DNS server

    // Advanced settings
    uint32_t connectionTimeoutMs = 15000; // Connection timeout
    bool enableAutoReconnect = true;   // Auto-reconnect on disconnect
};

// Screen Configuration
struct ScreenConfig {
    bool enableScreen = true;          // Enable screen output
    uint32_t updateIntervalMs = 1000;  // Screen update interval
    uint8_t maxLines = 8;              // Maximum lines to display
    bool enableAutoScroll = true;      // Auto-scroll old messages
    bool enableTimestamp = true;       // Show timestamps
    uint16_t messageTimeoutMs = 5000;  // Message display timeout
};

// Routing Configuration
struct RoutingConfig {
    bool enableRouting = true;         // Enable message routing
    uint32_t routingIntervalMs = 100;  // Routing task interval

    // Route definitions
    struct Route {
        Message::Type messageType;     // Message type to route
        TransportType sourceType;      // Source transport type
        TransportType destinationType; // Destination transport type
        bool enabled;                  // Route enabled
        uint8_t priority;              // Route priority (0=highest)
    };

    // Predefined routes
    Route routes[16];                  // Maximum 16 routes
    uint8_t routeCount = 0;            // Number of active routes
};

// Main Communication Configuration
struct CommunicationConfig {
    // Transport configurations
    UsbConfig usb;
    LoraConfig lora;
    WifiCommConfig wifi;
    ScreenConfig screen;

    // Routing configuration
    RoutingConfig routing;

    // Global settings
    bool enableCommunicationManager = true; // Enable the communication manager
    uint32_t updateIntervalMs = 100;        // Communication manager update interval
    uint8_t maxConcurrentMessages = 8;      // Maximum concurrent messages
    bool enableMessageBuffering = true;     // Enable message buffering
    uint16_t bufferSize = 1024;             // Message buffer size (bytes)

    // Factory method for default configuration
    static CommunicationConfig createDefault();
};

// Inline default implementation to ensure Arduino links it
inline CommunicationConfig CommunicationConfig::createDefault() {
    CommunicationConfig config;

    // USB defaults
    config.usb.enableDebug = true;
    config.usb.baudRate = 115200;
    config.usb.enableTimestamp = true;

    // LoRa defaults (EU868)
    config.lora.enableLora = true;
    config.lora.frequency = 868000000UL;
    config.lora.txPower = 14;
    config.lora.spreadingFactor = 7;

    // WiFi defaults
    config.wifi.enableWifi = true;
    config.wifi.ssid = "YourWiFiNetwork";
    config.wifi.password = "YourWiFiPassword";

    // Screen defaults
    config.screen.enableScreen = true;
    config.screen.updateIntervalMs = 1000;

    // Routing defaults
    config.routing.enableRouting = true;
    config.routing.routingIntervalMs = 100;

    return config;
}
