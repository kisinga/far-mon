#include "core_config.h"

// Unified configuration factory implementation
DeviceConfig DeviceConfigFactory::createBaseConfig(uint8_t deviceId, DeviceType deviceType) {
    DeviceConfig cfg{};
    cfg.deviceId = deviceId;
    cfg.deviceType = deviceType;

    // Set common defaults
    cfg.heartbeatIntervalMs = DEFAULT_HEARTBEAT_INTERVAL_MS;
    cfg.enableDisplay = true;
    cfg.displayUpdateIntervalMs = DEFAULT_DISPLAY_UPDATE_INTERVAL_MS;

    // Initialize communication configuration with neutral defaults
    cfg.communication = CommunicationConfig{};

    // Apply common communication settings (per-device may override)
    cfg.communication.enableCommunicationManager = false;
    cfg.communication.updateIntervalMs = DEFAULT_ROUTING_INTERVAL_MS;
    cfg.communication.maxConcurrentMessages = 8;
    cfg.communication.enableMessageBuffering = true;
    cfg.communication.bufferSize = 1024;

    // Apply common WiFi settings (can be overridden by device-specific configs)
    cfg.communication.wifi.reconnectIntervalMs = DEFAULT_WIFI_RECONNECT_INTERVAL_MS;
    cfg.communication.wifi.statusCheckIntervalMs = DEFAULT_WIFI_STATUS_CHECK_INTERVAL_MS;
    cfg.communication.wifi.maxReconnectAttempts = 10;
    cfg.communication.wifi.enableAutoReconnect = true;
    cfg.communication.wifi.connectionTimeoutMs = 15000;

    // Apply common LoRa settings
    cfg.communication.lora.maxPayload = 64;
    cfg.communication.lora.maxOutbox = 8;
    cfg.communication.lora.maxPeers = 16;
    cfg.communication.lora.ackTimeoutMs = 1500;
    cfg.communication.lora.maxRetries = 4;
    cfg.communication.lora.pingIntervalMs = 30000;
    cfg.communication.lora.masterTtlMs = 15000;

    // Apply common USB settings
    cfg.communication.usb.enableTimestamp = true;
    cfg.communication.usb.verboseLogging = true;
    cfg.communication.usb.rxBufferSize = 256;
    cfg.communication.usb.txBufferSize = 256;

    // Apply common screen settings
    cfg.communication.screen.maxLines = 8;
    cfg.communication.screen.enableAutoScroll = true;
    cfg.communication.screen.enableTimestamp = true;
    cfg.communication.screen.messageTimeoutMs = 5000;

    return cfg;
}

DeviceConfig DeviceConfigFactory::createRelayConfig(uint8_t deviceId) {
    DeviceConfig cfg = createBaseConfig(deviceId, DeviceType::Relay);

    // Relay-specific generic comm settings (no deployment secrets)
    cfg.communication.usb.enableDebug = true;
    cfg.communication.usb.baudRate = 115200;

    cfg.communication.lora.enableLora = true;
    cfg.communication.lora.frequency = 868000000UL;
    cfg.communication.lora.txPower = 14;

    // Set up routing rules for relay: LoRa -> WiFi, LoRa -> USB, Telemetry -> Screen
    cfg.communication.routing.enableRouting = true;
    cfg.communication.routing.routingIntervalMs = DEFAULT_ROUTING_INTERVAL_MS;

    // Add default routes (by transport type)
    cfg.communication.routing.routes[0] = {Messaging::Message::Type::Data, TransportType::LoRa, TransportType::WiFi, true, 0};        // LoRa -> WiFi
    cfg.communication.routing.routes[1] = {Messaging::Message::Type::Data, TransportType::LoRa, TransportType::USB_Debug, true, 1};    // LoRa -> USB
    cfg.communication.routing.routes[2] = {Messaging::Message::Type::Telemetry, TransportType::LoRa, TransportType::Screen, true, 0};  // LoRa -> Screen
    cfg.communication.routing.routeCount = 3;

    cfg.displayUpdateIntervalMs = 800;

    // MQTT topic/client are set by per-device configs
    return cfg;
}

DeviceConfig DeviceConfigFactory::createRemoteConfig(uint8_t deviceId) {
    DeviceConfig cfg = createBaseConfig(deviceId, DeviceType::Remote);

    // Remote-specific generic comm settings (no deployment secrets)
    cfg.communication.usb.enableDebug = true;
    cfg.communication.usb.baudRate = 115200;

    cfg.communication.wifi.enableWifi = false;  // Remotes typically don't need WiFi

    cfg.communication.lora.enableLora = true;
    cfg.communication.lora.frequency = 868000000UL;
    cfg.communication.lora.txPower = 14;

    // Set up routing rules for remote: Telemetry -> LoRa
    cfg.communication.routing.enableRouting = true;
    cfg.communication.routing.routingIntervalMs = DEFAULT_ROUTING_INTERVAL_MS;

    // Add default routes (by transport type)
    cfg.communication.routing.routes[0] = {Messaging::Message::Type::Telemetry, TransportType::Unknown, TransportType::LoRa, true, 0}; // Any -> LoRa
    cfg.communication.routing.routeCount = 1;

    cfg.displayUpdateIntervalMs = 200;
    return cfg;
}

// RelayConfig factory implementation
RelayConfig RelayConfig::create(uint8_t deviceId) {
    RelayConfig cfg{};
    static_cast<DeviceConfig&>(cfg) = DeviceConfigFactory::createRelayConfig(deviceId);
    cfg.deviceId = deviceId;  // Ensure deviceId is set correctly
    cfg.peerMonitorIntervalMs = 2000;
    cfg.maxPeers = 16;
    return cfg;
}

// RemoteConfig factory implementation
RemoteConfig RemoteConfig::create(uint8_t deviceId) {
    RemoteConfig cfg{};
    static_cast<DeviceConfig&>(cfg) = DeviceConfigFactory::createRemoteConfig(deviceId);
    cfg.deviceId = deviceId;  // Ensure deviceId is set correctly

    // Set remote-specific defaults
    cfg.enableAnalogSensor = true;
    cfg.analogInputPin = 34;
    cfg.analogReadIntervalMs = 200;
    cfg.telemetryReportIntervalMs = 2000;
    cfg.analogReferenceVoltage = 3.30f;
    cfg.masterNodeId = 1;
    cfg.useCalibratedAdc = true;

    return cfg;
}
