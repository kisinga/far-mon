// Simplified Relay Implementation - Uses Common Application Framework
// Much cleaner and more maintainable than the original

#include "../lib/common_app.h"
#include "../lib/device_config.h"
#include "relay_config.h"
#include "../lib/system_services.h"
#include "../lib/task_manager.h"
#include "../lib/display_provider.h"
#include "../lib/wifi_manager.h"
#include "../lib/wifi_config.h"
#include "../lib/logo.cpp"

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
    WifiManager wifiManager{wifiConfig};
    BatteryMonitor::BatteryMonitor batteryMonitor{batteryConfig};
    BatteryMonitor::Config batteryConfig;

    // Device-specific setup
    void setupDeviceConfig();
    void setupServices();
    void setupTasks();

    // Task implementations
    static void taskHeartbeat(CommonAppState& state);
    static void taskBatteryMonitor(CommonAppState& state);
    static void taskDisplayUpdate(CommonAppState& state);
    static void taskWifiMonitor(CommonAppState& state);
    static void taskLoRaUpdate(CommonAppState& state);
    static void taskPeerMonitor(CommonAppState& state);

    // Static service references (for tasks)
    static SystemServices* staticServices;
};

// Static member initialization
SystemServices* RelayApplication::staticServices = nullptr;

// Implementation
void RelayApplication::initialize() {
    setupDeviceConfig();
    setupServices();
    setupTasks();

    LOGI("relay", "Relay application initialized");
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
    oled.setHeaderRightMode(HeaderRightMode::PeerCount);
    relayHomeCtx.display = &oled;
    oled.setHomescreenRenderer(renderRelayHome, &relayHomeCtx);
    oled.setI2cClock(400000);
    bool found = oled.probeI2C(OLED_I2C_ADDR);
    Serial.printf("[i2c] OLED 0x%02X found=%s\n", OLED_I2C_ADDR, found ? "yes" : "no");
    if (!found) {
        oled.i2cScan(Serial);
    }

    // Initialize services
    services = SystemServices::create(oled, debugRouter, wifiManager, batteryMonitor, lora);
    staticServices = &services;

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
    taskManager.registerTask("lora", taskLoRaUpdate, config.loraTaskIntervalMs);
    taskManager.registerTask("wifi", taskWifiMonitor, 100);
    taskManager.registerTask("peer_monitor", taskPeerMonitor, config.peerMonitorIntervalMs);

    // Start RTOS scheduler (no-op on non-RTOS builds)
    taskManager.start(appState);

    gRelayReady = true;
    LOGI("relay", "Tasks registered");
}

// Task implementations
void RelayApplication::taskHeartbeat(CommonAppState& state) {
    state.heartbeatOn = !state.heartbeatOn;
    LOGI("relay", "Heartbeat: %s", state.heartbeatOn ? "ON" : "OFF");
}

void RelayApplication::taskBatteryMonitor(CommonAppState& state) {
    if (staticServices && staticServices->battery) {
        uint8_t percent = staticServices->battery->getBatteryPercent();
        bool charging = staticServices->battery->isCharging();
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
        LOGI("relay", "WiFi: %s", connected ? "CONNECTED" : "DISCONNECTED");
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
