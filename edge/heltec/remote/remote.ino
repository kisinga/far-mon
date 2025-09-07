// Simplified Remote Implementation - Uses Common Application Framework
// Much cleaner and more maintainable than the original

#include "../lib/common_app.h"
#include "../lib/device_config.h"
#include "remote_config.h"
#include "../lib/system_services.h"
#include "../lib/task_manager.h"
#include "../lib/display_provider.h"
#include "../lib/logo.cpp"
#include <memory>

struct HomeCtx { OledDisplay* display; };
static HomeCtx remoteHomeCtx{};
static volatile bool gRemoteReady = false;

// Simple homescreen renderer for quick visual confirmation
static void renderRemoteHome(SSD1306Wire &d, void *ctx) {
    int16_t cx = 52, cy = 14, cw = 76, ch = 50; // fallback offsets: logo 48px + 4px margin
    HomeCtx* c = static_cast<HomeCtx*>(ctx);
    if (c && c->display) {
        c->display->getContentArea(cx, cy, cw, ch);
    }
    d.setTextAlignment(TEXT_ALIGN_LEFT);
    d.drawString(cx, cy, F("Remote 03"));
    d.drawString(cx, cy + 14, gRemoteReady ? F("Ready") : F("Starting..."));
}

// Remote-specific state
struct RemoteAppState : CommonAppState {
    int analogRaw = 0;
    float analogVoltage = 0.0f;
    uint32_t nextReportDueMs = 0;
};

// Simplified remote application (combines framework + device-specific logic)
class RemoteApplication {
public:
    void initialize();
    void run();

private:
    // Configuration
    RemoteConfig config;

    // Framework components
    RemoteAppState appState;
    TaskManager taskManager{16};
    SystemServices services;

    // Hardware components
    OledDisplay oled;
    DebugRouter debugRouter;
    LoRaComm lora;
    std::unique_ptr<WifiManager> wifiManager;
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
    static void taskLoRaUpdate(CommonAppState& state);
    static void taskAnalogRead(CommonAppState& state);
    static void taskTelemetryReport(CommonAppState& state);

    // Static service references (for tasks)
    static SystemServices* staticServices;

    // Device-specific methods
    static float convertAdcToVolts(int adcRaw);
};

// Static member initialization
SystemServices* RemoteApplication::staticServices = nullptr;

// Implementation
void RemoteApplication::initialize() {
    setupDeviceConfig();
    setupServices();
    setupTasks();

    LOGI("remote", "Remote application initialized");
}

void RemoteApplication::run() {
    appState.nowMs = millis();
    taskManager.update(appState);
    delay(1);
}

void RemoteApplication::setupDeviceConfig() {
    config = createRemoteConfig("03");
}

void RemoteApplication::setupServices() {
    // Initialize hardware
    Serial.begin(115200);
    delay(200);
    Serial.println();

    // Initialize OLED display
    oled.begin(true);
    oled.setDeviceId("03");
    oled.setHeaderRightMode(HeaderRightMode::SignalBars);
    remoteHomeCtx.display = &oled;
    oled.setHomescreenRenderer(renderRemoteHome, &remoteHomeCtx);
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

    services = SystemServices::create(oled, debugRouter, *wifiManager, batteryMonitor, lora);
    staticServices = &services;

    // Initialize LoRa
    lora.begin(LoRaComm::Mode::Slave, 3);
    lora.setVerbose(false);
    lora.setLogLevel((uint8_t)Logger::Level::Info);

    // Initialize analog pin
    pinMode(config.analogInputPin, INPUT);

    // Seed random for jitter
    randomSeed((uint32_t)millis());

    // Initialize first report time
    const int32_t jitter = (int32_t)((int32_t)config.telemetryReportIntervalMs / 5);
    const int32_t delta = (int32_t)config.telemetryReportIntervalMs + (int32_t)random(-jitter, jitter + 1);
    RemoteAppState& remoteState = static_cast<RemoteAppState&>(appState);
    remoteState.nextReportDueMs = millis() + (uint32_t)max(100, (int)delta);

    LOGI("remote", "Services initialized");
}

void RemoteApplication::setupTasks() {
    // Register common tasks
    taskManager.registerTask("heartbeat", taskHeartbeat, config.heartbeatIntervalMs);
    taskManager.registerTask("battery", taskBatteryMonitor, 1000);
    taskManager.registerTask("display", taskDisplayUpdate, config.displayUpdateIntervalMs);
    taskManager.registerTask("lora", taskLoRaUpdate, 50);
    taskManager.registerTask("analog_read", taskAnalogRead, config.analogReadIntervalMs);
    taskManager.registerTask("telemetry", taskTelemetryReport, 100);

    // Start RTOS scheduler (no-op on non-RTOS builds)
    taskManager.start(appState);

    gRemoteReady = true;
    LOGI("remote", "Tasks registered");
}

// Task implementations
void RemoteApplication::taskHeartbeat(CommonAppState& state) {
    state.heartbeatOn = !state.heartbeatOn;
    LOGI("remote", "Heartbeat: %s", state.heartbeatOn ? "ON" : "OFF");
}

void RemoteApplication::taskBatteryMonitor(CommonAppState& state) {
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

        LOGI("remote", "Battery: %d%%, Charging: %s", percent, charging ? "YES" : "NO");
    }
}

void RemoteApplication::taskDisplayUpdate(CommonAppState& state) {
    if (staticServices && staticServices->display) {
        staticServices->display->tick(state.nowMs);
    }
}

void RemoteApplication::taskLoRaUpdate(CommonAppState& state) {
    if (staticServices && staticServices->lora) {
        staticServices->lora->update(state.nowMs);
        if (staticServices->oledDisplay) {
            staticServices->oledDisplay->setLoraStatus(staticServices->lora->isConnected(),
                                                       staticServices->lora->getLastRssiDbm());
        }
    }
}

void RemoteApplication::taskAnalogRead(CommonAppState& state) {
    // Read analog sensor - cast to access remote-specific fields
    RemoteAppState& remoteState = static_cast<RemoteAppState&>(state);
    const int raw = analogRead(34); // Using hardcoded pin for now
    remoteState.analogRaw = raw;
    remoteState.analogVoltage = convertAdcToVolts(raw);

    LOGI("remote", "Analog: raw=%d, voltage=%.3fV", raw, remoteState.analogVoltage);
}

void RemoteApplication::taskTelemetryReport(CommonAppState& state) {
    // Cast to access remote-specific fields
    RemoteAppState& remoteState = static_cast<RemoteAppState&>(state);

    // Check if it's time to report
    if ((int32_t)(millis() - remoteState.nextReportDueMs) < 0) {
        return;
    }

    // Send telemetry
    if (staticServices && staticServices->lora) {
        char buf[48];
        int n = snprintf(buf, sizeof(buf), "id=%s,r=%d,v=%.3f",
                        "03", remoteState.analogRaw, remoteState.analogVoltage);

        if (n > 0) {
            staticServices->lora->sendData(1, (const uint8_t*)buf,
                                          (uint8_t)min(n, (int)sizeof(buf) - 1), true);
            LOGI("remote", "Telemetry sent: %s", buf);
        }
    }

    // Schedule next report with jitter
    const int32_t jitter = (int32_t)((int32_t)2000 / 5); // 2 second interval
    const int32_t delta = 2000 + (int32_t)random(-jitter, jitter + 1);
    remoteState.nextReportDueMs = millis() + (uint32_t)max(100, (int)delta);
}

float RemoteApplication::convertAdcToVolts(int adcRaw) {
    const float scale = 1.0f / 4095.0f;
    return (float)adcRaw * scale * 3.30f; // 3.3V reference
}

// Global remote application instance
RemoteApplication remoteApp;

// Arduino setup
void setup() {
    remoteApp.initialize();
}

// Arduino main loop
void loop() {
    remoteApp.run();
}
