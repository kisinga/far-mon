// Simplified Relay Implementation - Uses Common Application Framework
// Much cleaner and more maintainable than the original

#include "lib/device_config.h"
#include "lib/system_services.h"
#include "lib/task_manager.h"
#include "lib/wifi_manager.h"
#include "lib/logger.h"
#include "lib/logo.cpp"
#include "lib/communication_manager.h"
#include "lib/transport_wifi.h"
#include "lib/transport_lora.h"
#include "lib/transport_usb.h"
#include "lib/transport_screen.h"
#include "lib/communication_logger.h"
#include "lib/mqtt_publisher.h"
#include "config.h"
#include <memory>

// UI Framework
#include "lib/ui_manager.h"
#include "lib/TextElement.h"
#include "lib/IconElement.h"
#include "lib/BatteryIconElement.h"
#include "lib/HeaderStatusElement.h"

// Source file includes for Arduino build system
#include "lib/ui_manager.cpp"
#include "lib/ScreenLayout.cpp"
#include "lib/TopBarLayout.cpp"
#include "lib/MainContentLayout.cpp"
#include "lib/BatteryIconElement.cpp"
#include "lib/HeaderStatusElement.cpp"

static volatile bool gRelayReady = false;

// Relay-specific state
struct RelayAppState : CommonAppState {
    // Add relay-specific state variables here
};

// Simplified relay application (combines framework + device-specific logic)
class RelayApplication {
public:
    void initialize();
    void run();

private:
    // Configuration
    RelayConfig config;

    // Framework components
    RelayAppState appState;
    TaskManager taskManager{16};
    SystemServices services;

    // Hardware components
    OledDisplay oled;
    UIManager uiManager{oled};
    LoRaComm lora;
    std::unique_ptr<WifiManager> wifiManager;
    BatteryMonitor::BatteryMonitor batteryMonitor{batteryConfig};
    BatteryMonitor::Config batteryConfig;

    // Communication Manager and Transports
    std::unique_ptr<CommunicationManager> commManager;
    std::unique_ptr<TransportWiFi> transportWifi;
    std::unique_ptr<TransportLoRa> transportLora;
    std::unique_ptr<TransportUSB> transportUsb;
    std::unique_ptr<TransportScreen> transportScreen;
    std::unique_ptr<MqttPublisher> mqtt;

    // UI Components
    TextElement* statusText = nullptr;
    BatteryIconElement* batteryIcon = nullptr;
    HeaderStatusElement* headerStatus = nullptr;

    // Device-specific setup
    void setupDeviceConfig();
    void setupServices();
    void setupTasks();
    void setupCommunicationManager();

    // Task implementations
    static void taskHeartbeat(CommonAppState& state);
    static void taskBatteryMonitor(CommonAppState& state);
    static void taskDisplayUpdate(CommonAppState& state);
    static void taskWifiMonitor(CommonAppState& state);
    static void taskLoRaUpdate(CommonAppState& state);
    static void taskPeerMonitor(CommonAppState& state);
    static void taskMqtt(CommonAppState& state);
    static void taskRouter(CommonAppState& state);
    static void taskCommunicationManager(CommonAppState& state);

    // Static service references (for tasks)
    static SystemServices* staticServices;
    static RelayConfig* staticConfig;
    static LoRaComm* staticLora;
    static WifiManager* staticWifi;
    static CommunicationManager* staticCommManager;
    static MqttPublisher* staticMqtt;
    static RelayApplication* staticApplication;
    // Simple routing ring buffer for LoRa->WiFi
    struct RouteMsg { uint8_t srcId; uint8_t len; uint8_t bytes[64]; };
    static constexpr size_t kRouteQueueSize = 8;
    static RouteMsg routeQueue[kRouteQueueSize];
    static volatile uint8_t routeHead;
    static volatile uint8_t routeTail;
    static void onLoraData(uint8_t srcId, const uint8_t* payload, uint8_t length);
};

// Static member initialization
SystemServices* RelayApplication::staticServices = nullptr;
RelayConfig* RelayApplication::staticConfig = nullptr;
LoRaComm* RelayApplication::staticLora = nullptr;
WifiManager* RelayApplication::staticWifi = nullptr;
CommunicationManager* RelayApplication::staticCommManager = nullptr;
MqttPublisher* RelayApplication::staticMqtt = nullptr;
RelayApplication* RelayApplication::staticApplication = nullptr;
RelayApplication::RouteMsg RelayApplication::routeQueue[RelayApplication::kRouteQueueSize] = {};
volatile uint8_t RelayApplication::routeHead = 0;
volatile uint8_t RelayApplication::routeTail = 0;

// Implementation
void RelayApplication::initialize() {
    setupDeviceConfig();
    setupServices();
    setupTasks();

    staticApplication = this;

    CommunicationLogger::info("relay", "Relay application initialized");
}

void RelayApplication::run() {
    appState.nowMs = millis();
    taskManager.update(appState);
    delay(1);
}

void RelayApplication::setupDeviceConfig() {
    config = buildRelayConfig();
}

void RelayApplication::setupServices() {
    // Initialize hardware
    Serial.begin(115200);
    delay(200);
    Serial.println();

    // Initialize OLED display
    oled.safeBegin(true);
    oled.setI2cClock(400000);
    bool found = oled.probeI2C(OLED_I2C_ADDR);
    Serial.printf("[i2c] OLED 0x%02X found=%s\n", OLED_I2C_ADDR, found ? "yes" : "no");
    if (!found) {
        oled.i2cScan(Serial);
    }

    // Setup UI
    uiManager.init();
    auto& layout = uiManager.getLayout();

    // Top bar
    layout.getTopBar().setColumn(0, std::make_unique<TextElement>(String("Relay ") + RELAY_DEVICE_ID));
    
    auto batteryElement = std::make_unique<BatteryIconElement>();
    batteryIcon = batteryElement.get();
    layout.getTopBar().setColumn(1, std::move(batteryElement));

    auto headerStatusElement = std::make_unique<HeaderStatusElement>();
    headerStatus = headerStatusElement.get();
    headerStatus->setMode(HeaderStatusElement::Mode::PeerCount);
    layout.getTopBar().setColumn(3, std::move(headerStatusElement));

    // Main content
    layout.getMainContent().setLeftColumnWidth(logo_small_width + 4);
    layout.getMainContent().setLeft(std::make_unique<IconElement>(logo_small_width, logo_small_height, logo_small_bits));
    auto statusTextElement = std::make_unique<TextElement>("Starting...");
    statusText = statusTextElement.get();
    layout.getMainContent().setRight(std::move(statusTextElement));

    // Initialize Logger (with display support for debug overlays)
    Logger::safeInitialize(&oled, RELAY_DEVICE_ID);

    // Initialize services
    // Initialize WiFi manager using centralized communication config
    {
        WifiManager::Config wmConfig;
        wmConfig.ssid = config.communication.wifi.ssid;
        wmConfig.password = config.communication.wifi.password;
        wmConfig.reconnectIntervalMs = config.communication.wifi.reconnectIntervalMs;
        wmConfig.statusCheckIntervalMs = config.communication.wifi.statusCheckIntervalMs;
        wifiManager = std::make_unique<WifiManager>(wmConfig);
        if (config.communication.wifi.enableWifi) {
            wifiManager->safeBegin();
        }
    }

    // Create services using a guaranteed WifiManager instance (may be uninitialized)
    services = SystemServices::create(oled, *wifiManager, batteryMonitor, lora);
    staticWifi = wifiManager.get();

    staticConfig = &config;
    staticLora = &lora;
    // Register LoRa data callback for routing
    lora.setOnDataReceived(&RelayApplication::onLoraData);
    staticServices = &services;

    // Initialize MQTT publisher if enabled
    {
        MqttPublisherConfig mc{};
        mc.enableMqtt = config.communication.mqtt.enableMqtt;
        mc.brokerHost = config.communication.mqtt.brokerHost;
        mc.brokerPort = config.communication.mqtt.brokerPort;
        mc.clientId = config.communication.mqtt.clientId;
        mc.username = config.communication.mqtt.username;
        mc.password = config.communication.mqtt.password;
        mc.baseTopic = config.communication.mqtt.baseTopic;
        // Use configured deviceTopic if provided; otherwise topicSuffix will be used at publish
        mc.deviceTopic = config.communication.mqtt.deviceTopic;
        mc.qos = config.communication.mqtt.qos;
        mc.retain = config.communication.mqtt.retain;
        mqtt = std::make_unique<MqttPublisher>(mc);
        mqtt->begin();
        staticMqtt = mqtt.get();
        CommunicationLogger::info("relay", "MQTT %s host=%s port=%u topic=%s/%s",
                                   mc.enableMqtt ? "ENABLED" : "DISABLED",
                                   mc.brokerHost ? mc.brokerHost : "(null)",
                                   (unsigned)mc.brokerPort,
                                   mc.baseTopic ? mc.baseTopic : "(null)",
                                   mc.deviceTopic ? mc.deviceTopic : "(auto)");
    }

    // Initialize Communication Manager if enabled
    if (config.communication.enableCommunicationManager) {
        setupCommunicationManager();
    }

    // Initialize LoRa
    lora.safeBegin(LoRaComm::Mode::Master, 1);
    lora.setVerbose(false);
    lora.setLogLevel((uint8_t)Logger::Level::Info);

    LOGI("relay", "Services initialized");
}

void RelayApplication::setupTasks() {
    // Register common tasks
    taskManager.registerTask("heartbeat", taskHeartbeat, config.heartbeatIntervalMs);
    taskManager.registerTask("battery", taskBatteryMonitor, 1000);
    taskManager.registerTask("display", taskDisplayUpdate, config.displayUpdateIntervalMs);
    taskManager.registerTask("lora", taskLoRaUpdate, 50);
    if (config.communication.wifi.enableWifi) {
        taskManager.registerTask("wifi", taskWifiMonitor, config.communication.wifi.statusCheckIntervalMs);
    }
    // Enable router task for LoRa->WiFi message forwarding
    taskManager.registerTask("router", taskRouter, 100);
    if (config.communication.enableCommunicationManager) {
        taskManager.registerTask("comm_mgr", taskCommunicationManager, config.communication.updateIntervalMs);
    }
    taskManager.registerTask("peer_monitor", taskPeerMonitor, config.peerMonitorIntervalMs);
    // MQTT maintenance
    taskManager.registerTask("mqtt", taskMqtt, 200);

    // Start RTOS scheduler (no-op on non-RTOS builds)
    taskManager.start(appState);

    gRelayReady = true;
    if (statusText) {
        statusText->setText("Ready");
    }
    LOGI("relay", "Tasks registered");
}

void RelayApplication::setupCommunicationManager() {
    // Create communication manager
    commManager = std::make_unique<CommunicationManager>();

    // Create and register transports using centralized communication config
    if (config.communication.wifi.enableWifi && wifiManager) {
        transportWifi = std::make_unique<TransportWiFi>(1, config.communication.wifi);
        commManager->registerTransport(transportWifi.get());
    }

    if (config.communication.lora.enableLora) {
        transportLora = std::make_unique<TransportLoRa>(2, lora, LoRaComm::Mode::Master, 1, config.communication.lora);
        TransportLoRa::setInstance(transportLora.get()); // Set static instance for callbacks
        commManager->registerTransport(transportLora.get());
    }

    if (config.communication.usb.enableDebug) {
        transportUsb = std::make_unique<TransportUSB>(3, config.communication.usb);
        commManager->registerTransport(transportUsb.get());
    }

    if (config.communication.screen.enableScreen) {
        transportScreen = std::make_unique<TransportScreen>(4, &oled, config.communication.screen);
        commManager->registerTransport(transportScreen.get());
    }

    // Set up routing rules from centralized configuration
    for (uint8_t i = 0; i < config.communication.routing.routeCount; i++) {
        const auto& route = config.communication.routing.routes[i];
        if (route.enabled) {
            MessageRouter::RoutingRule rule(
                route.messageType,
                route.sourceType,
                route.destinationType,
                false,
                true
            );
            commManager->addRoutingRule(rule);
        }
    }

    staticCommManager = commManager.get();

    // Initialize communication logger
    CommunicationLogger::begin(commManager.get(), staticConfig ? staticConfig->deviceId : RELAY_DEVICE_ID);

    CommunicationLogger::info("relay", "Communication Manager initialized with %d transports", commManager->getTransportCount());
}

// Task implementations
void RelayApplication::taskHeartbeat(CommonAppState& state) {
    state.heartbeatOn = !state.heartbeatOn;
    LOGI("relay", "Heartbeat: %s", state.heartbeatOn ? "ON" : "OFF");
}

void RelayApplication::taskBatteryMonitor(CommonAppState& state) {
    if (staticServices && staticServices->battery) {
        // Update charge detection state and read percent
        staticServices->battery->update(state.nowMs);
        uint8_t percent = staticServices->battery->getBatteryPercent();
        bool charging = staticServices->battery->isCharging();

        // Push to display so header battery icon reflects current state
        if (staticServices->oledDisplay) { // Keep this for now for backward compat with logger
            // The new component will handle the drawing
        }
        if (staticApplication && staticApplication->batteryIcon) {
            staticApplication->batteryIcon->setStatus(percent, charging);
        }

        LOGI("relay", "Battery: %d%%, Charging: %s", percent, charging ? "YES" : "NO");
    }
}

void RelayApplication::taskDisplayUpdate(CommonAppState& state) {
    if (staticApplication) {
        staticApplication->uiManager.tick();
    }
}

void RelayApplication::taskWifiMonitor(CommonAppState& state) {
    if (staticServices && staticServices->wifi) {
        staticServices->wifi->update(state.nowMs);
        bool connected = staticServices->wifi->isConnected();
        if (staticApplication && staticApplication->headerStatus) {
            // NOTE: The relay uses PeerCount mode, so this won't be visible unless mode is changed.
            int8_t percent = staticServices->wifi->getSignalStrengthPercent();
            staticApplication->headerStatus->setWifiStatus(connected, percent);
        }
        CommunicationLogger::info("relay", "WiFi: %s", connected ? "CONNECTED" : "DISCONNECTED");
    }
}

void RelayApplication::taskLoRaUpdate(CommonAppState& state) {
    if (staticServices && staticServices->loraComm) {
        staticServices->loraComm->tick(state.nowMs);
        Radio.IrqProcess();
        // Push LoRa status into header UI
        if (staticApplication && staticApplication->headerStatus) {
            staticApplication->headerStatus->setLoraStatus(staticServices->loraComm->isConnected(),
                                                      staticServices->loraComm->getLastRssiDbm());
            // Also update peer count since that's the visible mode
            size_t peerCount = 0;
            for (size_t i = 0; ; i++) {
                LoRaComm::PeerInfo p{};
                if (!staticServices->loraComm->getPeerByIndex(i, p)) break;
                if (p.connected) peerCount++;
            }
            staticApplication->headerStatus->setPeerCount((uint16_t)peerCount);
        }
    }
}

void RelayApplication::taskPeerMonitor(CommonAppState& state) {
    if (staticServices && staticServices->loraComm) {
        size_t peerCount = 0;
        for (size_t i = 0; ; i++) {
            LoRaComm::PeerInfo p{};
            if (!staticServices->loraComm->getPeerByIndex(i, p)) break;
            if (p.connected) peerCount++;
        }
        LOGI("relay", "Connected peers: %u", (unsigned)peerCount);
        if (staticApplication && staticApplication->headerStatus) {
            staticApplication->headerStatus->setPeerCount((uint16_t)peerCount);
        }
    }
}

void RelayApplication::taskMqtt(CommonAppState& state) {
    (void)state;
    if (staticMqtt) {
        staticMqtt->update(millis());
    }
}

void RelayApplication::taskRouter(CommonAppState& state) {
    (void)state;
    if (!staticConfig || !staticLora || !staticWifi) return;
    // Drain a limited number of messages per tick and uplink via MQTT/WiFi
    uint8_t processed = 0;
    while (routeHead != routeTail && processed < 4) {
        RouteMsg msg;
        noInterrupts();
        msg = routeQueue[routeTail];
        routeTail = (uint8_t)((routeTail + 1) % kRouteQueueSize);
        interrupts();
        if (msg.len > 0) {
            // Prefer MQTT if available and ready
            if (staticMqtt && staticMqtt->isReady()) {
                char topicSuffix[16];
                snprintf(topicSuffix, sizeof(topicSuffix), "%u", (unsigned)msg.srcId);
                staticMqtt->publish(topicSuffix, msg.bytes, msg.len);
            } else {
                // Fallback: WiFi uplink (placeholder)
                (void)staticWifi->uplink(msg.bytes, msg.len);
            }
        }
        processed++;
    }
}

void RelayApplication::taskCommunicationManager(CommonAppState& state) {
    if (staticCommManager) {
        staticCommManager->update(state.nowMs);
    }
}

void RelayApplication::onLoraData(uint8_t srcId, const uint8_t* payload, uint8_t length) {
    if (!payload || length == 0) return;
    if (length > sizeof(RouteMsg::bytes)) length = sizeof(RouteMsg::bytes);
    uint8_t nextHead = (uint8_t)((routeHead + 1) % kRouteQueueSize);
    if (nextHead == routeTail) {
        // queue full, drop oldest
        routeTail = (uint8_t)((routeTail + 1) % kRouteQueueSize);
    }
    noInterrupts();
    routeQueue[routeHead].srcId = srcId;
    routeQueue[routeHead].len = length;
    memcpy(routeQueue[routeHead].bytes, payload, length);
    routeHead = nextHead;
    interrupts();
}

// Global relay application instance
RelayApplication relayApp;

// Arduino setup
void setup() {
    relayApp.initialize();
}

// Arduino main loop
void loop() {
    relayApp.run();
}
