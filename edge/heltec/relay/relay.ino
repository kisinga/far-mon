// Simplified Relay Implementation - Uses Common Application Framework
// Much cleaner and more maintainable than the original

#include "../lib/common_app.h"
#include "../lib/device_config.h"
#include "relay_config.h"
#include "../lib/system_services.h"
#include "../lib/task_manager.h"
#include "../lib/display_provider.h"
#include "../lib/wifi_manager.h"
#include "../lib/logo.cpp"
#include "../lib/communication_manager.h"
#include "../lib/communication_logger.h"
#include "../lib/transport_wifi.h"
#include "../lib/transport_lora.h"
#include "../lib/transport_usb.h"
#include "../lib/transport_screen.h"
#include <memory>

struct HomeCtx { OledDisplay* display; };
static HomeCtx relayHomeCtx{};
static volatile bool gRelayReady = false;

// Simple homescreen renderer for quick visual confirmation
static void renderRelayHome(SSD1306Wire &d, void *ctx) {
    int16_t cx = 52, cy = 14, cw = 76, ch = 50; // fallback offsets: logo 48px + 4px margin
    HomeCtx* c = static_cast<HomeCtx*>(ctx);
    if (c && c->display) {
        c->display->getContentArea(cx, cy, cw, ch);
    }
    d.setTextAlignment(TEXT_ALIGN_LEFT);
    d.drawString(cx, cy, F("Relay 01"));
    d.drawString(cx, cy + 14, gRelayReady ? F("Ready") : F("Starting..."));
}

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
    DebugRouter debugRouter;
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
    static void taskRouter(CommonAppState& state);
    static void taskCommunicationManager(CommonAppState& state);

    // Static service references (for tasks)
    static SystemServices* staticServices;
    static RelayConfig* staticConfig;
    static LoRaComm* staticLora;
    static WifiManager* staticWifi;
    static CommunicationManager* staticCommManager;
    // Simple routing ring buffer for LoRa->WiFi
    struct RouteMsg { uint8_t bytes[64]; uint8_t len; };
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
RelayApplication::RouteMsg RelayApplication::routeQueue[RelayApplication::kRouteQueueSize] = {};
volatile uint8_t RelayApplication::routeHead = 0;
volatile uint8_t RelayApplication::routeTail = 0;

// Implementation
void RelayApplication::initialize() {
    setupDeviceConfig();
    setupServices();
    setupTasks();

    CommunicationLogger::info("relay", "Relay application initialized");
}

void RelayApplication::run() {
    appState.nowMs = millis();
    taskManager.update(appState);
    delay(1);
}

void RelayApplication::setupDeviceConfig() {
    config = createRelayConfig("01");
}

void RelayApplication::setupServices() {
    // Initialize hardware
    Serial.begin(115200);
    delay(200);
    Serial.println();

    // Initialize OLED display
    oled.begin(true);
    oled.setDeviceId("01");
    // Keep peer count on relay header (right side)
    oled.setHeaderRightMode(HeaderRightMode::PeerCount);
    // Also show a small WiFi icon in the header-left near battery
    oled.setShowWifiMiniIconInHeaderLeft(true);
    relayHomeCtx.display = &oled;
    oled.setHomescreenRenderer(renderRelayHome, &relayHomeCtx);
    oled.setI2cClock(400000);
    bool found = oled.probeI2C(OLED_I2C_ADDR);
    Serial.printf("[i2c] OLED 0x%02X found=%s\n", OLED_I2C_ADDR, found ? "yes" : "no");
    if (!found) {
        oled.i2cScan(Serial);
    }

    // Initialize services
    // Initialize WiFi manager using centralized communication config
    if (config.communication.wifi.enableWifi) {
        WifiManager::Config wmConfig;
        wmConfig.ssid = config.communication.wifi.ssid;
        wmConfig.password = config.communication.wifi.password;
        wmConfig.reconnectIntervalMs = config.communication.wifi.reconnectIntervalMs;
        wmConfig.statusCheckIntervalMs = config.communication.wifi.statusCheckIntervalMs;
        wifiManager = std::make_unique<WifiManager>(wmConfig);
        wifiManager->begin();
    }

    // Create services - WiFi manager might be null if disabled
    if (wifiManager) {
        services = SystemServices::create(oled, debugRouter, *wifiManager, batteryMonitor, lora);
        staticWifi = wifiManager.get();
    } else {
        // TODO: Need to update SystemServices::create to handle optional WiFi
        // For now, create with a dummy WiFi manager
        WifiManager dummyWifi({nullptr, nullptr, 30000, 5000});
        services = SystemServices::create(oled, debugRouter, dummyWifi, batteryMonitor, lora);
        staticWifi = nullptr;
    }

    staticConfig = &config;
    staticLora = &lora;
    // Register LoRa data callback for routing
    lora.setOnDataReceived(&RelayApplication::onLoraData);
    staticServices = &services;

    // Initialize Communication Manager if enabled
    if (config.communication.enableCommunicationManager) {
        setupCommunicationManager();
    }

    // Initialize LoRa
    lora.begin(LoRaComm::Mode::Master, 1);
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
    if (false) {
        taskManager.registerTask("router", taskRouter, 100);
    }
    if (config.communication.enableCommunicationManager) {
        taskManager.registerTask("comm_mgr", taskCommunicationManager, config.communication.updateIntervalMs);
    }
    taskManager.registerTask("peer_monitor", taskPeerMonitor, config.peerMonitorIntervalMs);

    // Start RTOS scheduler (no-op on non-RTOS builds)
    taskManager.start(appState);

    gRelayReady = true;
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
        transportLora = std::make_unique<TransportLoRa>(2, LoRaComm::Mode::Master, 1, config.communication.lora);
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
                static_cast<TransportType>(route.sourceTransportId),
                static_cast<TransportType>(route.destinationTransportId),
                false, // ack required
                true   // enabled
            );
            commManager->addRoutingRule(rule);
        }
    }

    staticCommManager = commManager.get();

    // Initialize communication logger
    CommunicationLogger::begin(commManager.get(), "01");

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
        if (staticServices->display) {
            staticServices->display->setBatteryStatus(percent <= 100, percent);
            staticServices->display->setBatteryCharging(charging);
        }

        LOGI("relay", "Battery: %d%%, Charging: %s", percent, charging ? "YES" : "NO");
    }
}

void RelayApplication::taskDisplayUpdate(CommonAppState& state) {
    if (staticServices && staticServices->display) {
        staticServices->display->tick(state.nowMs);
    }
}

void RelayApplication::taskWifiMonitor(CommonAppState& state) {
    if (staticServices && staticServices->wifi) {
        staticServices->wifi->update(state.nowMs);
        bool connected = staticServices->wifi->isConnected();
        if (staticServices->oledDisplay) {
            // Map RSSI percent via WifiManager helper and push to header
            int8_t percent = staticServices->wifi->getSignalStrengthPercent();
            staticServices->oledDisplay->setWifiStatus(connected, percent);
        }
        CommunicationLogger::info("relay", "WiFi: %s", connected ? "CONNECTED" : "DISCONNECTED");
    }
}

void RelayApplication::taskLoRaUpdate(CommonAppState& state) {
    if (staticServices && staticServices->lora) {
        staticServices->lora->update(state.nowMs);
        // Push LoRa status into header UI
        if (staticServices->oledDisplay) {
            // Even though relay shows peer count, set LoRa status too (no harm)
            staticServices->oledDisplay->setLoraStatus(staticServices->lora->isConnected(),
                                                       staticServices->lora->getLastRssiDbm());
            staticServices->oledDisplay->setPeerCount((uint16_t)staticServices->lora->getPeerCount());
        }
    }
}

void RelayApplication::taskPeerMonitor(CommonAppState& state) {
    if (staticServices && staticServices->lora) {
        size_t peerCount = staticServices->lora->getPeerCount();
        LOGI("relay", "Connected peers: %u", (unsigned)peerCount);
        if (staticServices->oledDisplay) {
            staticServices->oledDisplay->setPeerCount((uint16_t)peerCount);
        }
    }
}

void RelayApplication::taskRouter(CommonAppState& state) {
    (void)state;
    if (!staticConfig || !staticLora || !staticWifi) return;
    // WiFi dependency removed; to reintroduce, use CommunicationConfig routing rules
    // Drain route queue and uplink via WiFi
    while (routeHead != routeTail) {
        RouteMsg msg;
        noInterrupts();
        msg = routeQueue[routeTail];
        routeTail = (uint8_t)((routeTail + 1) % kRouteQueueSize);
        interrupts();
        if (msg.len > 0) {
            (void)staticWifi->uplink(msg.bytes, msg.len);
        }
    }
}

void RelayApplication::taskCommunicationManager(CommonAppState& state) {
    if (staticCommManager) {
        staticCommManager->update(state.nowMs);
    }
}

void RelayApplication::onLoraData(uint8_t srcId, const uint8_t* payload, uint8_t length) {
    (void)srcId;
    if (!payload || length == 0) return;
    if (length > sizeof(RouteMsg::bytes)) length = sizeof(RouteMsg::bytes);
    uint8_t nextHead = (uint8_t)((routeHead + 1) % kRouteQueueSize);
    if (nextHead == routeTail) {
        // queue full, drop oldest
        routeTail = (uint8_t)((routeTail + 1) % kRouteQueueSize);
    }
    noInterrupts();
    memcpy(routeQueue[routeHead].bytes, payload, length);
    routeQueue[routeHead].len = length;
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
