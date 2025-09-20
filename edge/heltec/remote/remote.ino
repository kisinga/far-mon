// Simplified Remote Implementation - Uses Common Application Framework
// Much cleaner and more maintainable than the original

#include "lib/device_config.h"
#include "lib/system_services.h"
#include "lib/task_manager.h"
#include "lib/logger.h"
#include "lib/lora_comm.h"
#include "lib/battery_monitor.h"
#include "lib/ui_manager.h"
#include "lib/TextElement.h"
#include "lib/IconElement.h"
#include "lib/BatteryIconElement.h"
#include "lib/HeaderStatusElement.h"
#include "lib/logo.h"
#include "sensor_interface.h"
#include "sensor_implementations.h"
#include "remote_sensor_config.h"
#include "config.h"
#include <memory>

// Source file includes for Arduino build system
#include "lib/ui_manager.cpp"
#include "lib/ScreenLayout.cpp"
#include "lib/TopBarLayout.cpp"
#include "lib/MainContentLayout.cpp"
#include "lib/BatteryIconElement.cpp"
#include "lib/HeaderStatusElement.cpp"

// struct HomeCtx { OledDisplay* display; };
// static HomeCtx remoteHomeCtx{};
static volatile bool gRemoteReady = false;

// Simple homescreen renderer for quick visual confirmation
// static void renderRemoteHome(SSD1306Wire &d, void *ctx) {
//     int16_t cx = 52, cy = 14, cw = 76, ch = 50; // fallback offsets: logo 48px + 4px margin
//     HomeCtx* c = static_cast<HomeCtx*>(ctx);
//     if (c && c->display) {
//         c->display->getContentArea(cx, cy, cw, ch);
//     }
//     d.setTextAlignment(TEXT_ALIGN_LEFT);
//     char header[16];
//     snprintf(header, sizeof(header), "Remote %s", REMOTE_DEVICE_ID);
//     d.drawString(cx, cy, header);
//     d.drawString(cx, cy + 14, gRemoteReady ? F("Ready") : F("Starting..."));
// }

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
    RemoteSensorConfig sensorConfig;

    // Framework components
    RemoteAppState appState;
    TaskManager taskManager{16};
    SystemServices services;

    // Hardware components
    OledDisplay oled;
    UIManager uiManager{oled};
    LoRaComm lora;
    std::unique_ptr<WifiManager> wifiManager;
    BatteryMonitor::Config batteryConfig;
    BatteryMonitor::BatteryMonitor batteryMonitor{batteryConfig};

    // Sensor system components
    SensorManager sensorManager;
    std::unique_ptr<LoRaBatchTransmitter> sensorTransmitter;

    // UI Components
    TextElement* statusText = nullptr;
    BatteryIconElement* batteryIcon = nullptr;
    HeaderStatusElement* headerStatus = nullptr;

    // Device-specific setup
    void setupDeviceConfig();
    void setupServices();
    void setupTasks();
    void setupSensors();

    // Task implementations
    static void taskHeartbeat(CommonAppState& state);
    static void taskBatteryMonitor(CommonAppState& state);
    static void taskDisplayUpdate(CommonAppState& state);
    static void taskLoRaUpdate(CommonAppState& state);
    static void taskAnalogRead(CommonAppState& state);
    static void taskTelemetryReport(CommonAppState& state);
    static void taskSensorUpdate(CommonAppState& state);

    // Static service references (for tasks)
    static SystemServices* staticServices;
    static RemoteConfig* staticConfig;
    static RemoteApplication* staticApplication;

    // Device-specific methods
    static float convertAdcToVolts(int adcRaw);
};

// Static member initialization
SystemServices* RemoteApplication::staticServices = nullptr;
RemoteConfig* RemoteApplication::staticConfig = nullptr;
RemoteApplication* RemoteApplication::staticApplication = nullptr;

// Implementation
void RemoteApplication::initialize() {
    setupDeviceConfig();
    setupServices();
    setupSensors();
    setupTasks();

    // Set static reference for tasks
    staticApplication = this;

    LOGI("remote", "Remote application initialized");
}

void RemoteApplication::run() {
    appState.nowMs = millis();
    taskManager.update(appState);
    delay(1);
}

void RemoteApplication::setupDeviceConfig() {
    config = buildRemoteConfig();
    sensorConfig = buildRemoteSensorConfig();
}

void RemoteApplication::setupServices() {
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
    layout.getTopBar().setColumn(0, std::make_unique<TextElement>(String("ID:") + REMOTE_DEVICE_ID));
    
    auto batteryElement = std::make_unique<BatteryIconElement>();
    batteryIcon = batteryElement.get();
    layout.getTopBar().setColumn(1, std::move(batteryElement));

    auto headerStatusElement = std::make_unique<HeaderStatusElement>();
    headerStatus = headerStatusElement.get();
    layout.getTopBar().setColumn(3, std::move(headerStatusElement));


    // Main content
    layout.getMainContent().setLeftColumnWidth(logo_small_width + 4); // logo width + margin
    layout.getMainContent().setLeft(std::make_unique<IconElement>(logo_small_width, logo_small_height, logo_small_bits));
    auto statusTextElement = std::make_unique<TextElement>("Starting...");
    statusText = statusTextElement.get();
    layout.getMainContent().setRight(std::move(statusTextElement));


    // Initialize Logger (with display support for debug overlays)
    Logger::safeInitialize(&oled, REMOTE_DEVICE_ID);

    // Initialize services
    // Always create a WifiManager instance; enable begin() only if WiFi is enabled
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

    services = SystemServices::create(oled, *wifiManager, batteryMonitor, lora);
    staticServices = &services;
    staticConfig = &config;

    // Initialize LoRa
    lora.safeBegin(LoRaComm::Mode::Slave, 3);
    lora.setVerbose(false);
    lora.setLogLevel((uint8_t)Logger::Level::Info);
    // Enable auto ping so relay can track presence reliably after reconnects
    lora.setAutoPingEnabled(true);

    // Initialize analog input only if enabled and pin configured
    if (config.enableAnalogSensor && config.analogInputPin != 0xFF) {
        pinMode(config.analogInputPin, INPUT);
        // Configure ADC attenuation for better voltage range (0-3.3V)
        analogSetPinAttenuation(config.analogInputPin, ADC_11db);
    }

    // Seed random for jitter
    randomSeed((uint32_t)millis());

    // Initialize first report time (respect configured interval)
    const int32_t jitter = (int32_t)((int32_t)config.telemetryReportIntervalMs / 5);
    const int32_t delta = (int32_t)config.telemetryReportIntervalMs + (int32_t)random(-jitter, jitter + 1);
    RemoteAppState& remoteState = static_cast<RemoteAppState&>(appState);
    remoteState.nextReportDueMs = millis() + (uint32_t)max(100, (int)delta);

    LOGI("remote", "Services initialized");
}

void RemoteApplication::setupSensors() {
    if (!sensorConfig.enableSensorSystem) {
        LOGI("remote", "Sensor system disabled");
        return;
    }

    LOGI("remote", "Setting up sensor system...");

    // Create LoRa transmitter for sensor data
    sensorTransmitter = std::make_unique<LoRaBatchTransmitter>(&lora, REMOTE_DEVICE_ID);
    sensorManager.setTransmitter(std::move(sensorTransmitter));

    // Configure and add sensors based on sensor config
    if (sensorConfig.ultrasonicConfig.enabled) {
        auto ultrasonic = SensorFactory::createUltrasonicSensor(
            sensorConfig.ultrasonicConfig,
            sensorConfig.pins.ultrasonicTrig,
            sensorConfig.pins.ultrasonicEcho
        );
        if (sensorManager.addSensor(std::move(ultrasonic))) {
            LOGI("remote", "Ultrasonic sensor enabled (pins: %d,%d)",
                 sensorConfig.pins.ultrasonicTrig, sensorConfig.pins.ultrasonicEcho);
        }
    }

    if (sensorConfig.waterLevelConfig.enabled) {
        auto waterLevel = SensorFactory::createWaterLevelSensor(
            sensorConfig.waterLevelConfig,
            sensorConfig.pins.waterLevel
        );
        if (sensorManager.addSensor(std::move(waterLevel))) {
            LOGI("remote", "Water level sensor enabled (pin: %d)", sensorConfig.pins.waterLevel);
        }
    }

    if (sensorConfig.waterFlowConfig.enabled) {
        auto waterFlow = SensorFactory::createWaterFlowSensor(
            sensorConfig.waterFlowConfig,
            sensorConfig.pins.waterFlow
        );
        if (sensorManager.addSensor(std::move(waterFlow))) {
            LOGI("remote", "Water flow sensor enabled (pin: %d)", sensorConfig.pins.waterFlow);
        }
    }

    if (sensorConfig.rs485Config.enabled) {
        auto rs485 = SensorFactory::createRS485Sensor(
            sensorConfig.rs485Config,
            Serial1, // Use Serial1 for RS485
            sensorConfig.pins.rs485RE,
            sensorConfig.pins.rs485DE
        );
        if (sensorManager.addSensor(std::move(rs485))) {
            LOGI("remote", "RS485 sensor enabled (RE: %d, DE: %d)",
                 sensorConfig.pins.rs485RE, sensorConfig.pins.rs485DE);
        }
    }

    if (sensorConfig.tempHumidityConfig.enabled) {
        auto tempHumidity = SensorFactory::createTempHumiditySensor(
            sensorConfig.tempHumidityConfig,
            sensorConfig.pins.tempHumidity
        );
        if (sensorManager.addSensor(std::move(tempHumidity))) {
            LOGI("remote", "Temperature/Humidity sensor enabled (pin: %d)", sensorConfig.pins.tempHumidity);
        }
    }

    LOGI("remote", "Sensor system setup complete with %d sensors", sensorManager.getSensorCount());
}

void RemoteApplication::setupTasks() {
    // Register common tasks
    taskManager.registerTask("heartbeat", taskHeartbeat, config.heartbeatIntervalMs);
    taskManager.registerTask("battery", taskBatteryMonitor, 1000);
    taskManager.registerTask("display", taskDisplayUpdate, config.displayUpdateIntervalMs);
    taskManager.registerTask("lora", taskLoRaUpdate, 50);
    taskManager.registerTask("analog_read", taskAnalogRead, config.analogReadIntervalMs);
    taskManager.registerTask("telemetry", taskTelemetryReport, 100);

    // Register sensor task if sensor system is enabled
    if (sensorConfig.enableSensorSystem) {
        taskManager.registerTask("sensors", taskSensorUpdate, 1000); // Check sensors every second
    }

    // Start RTOS scheduler (no-op on non-RTOS builds)
    taskManager.start(appState);

    gRemoteReady = true;
    if (statusText) {
        statusText->setText("Ready");
    }
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
        if (staticApplication && staticApplication->batteryIcon) {
            staticApplication->batteryIcon->setStatus(percent, charging);
        }

        LOGI("remote", "Battery: %d%%, Charging: %s", percent, charging ? "YES" : "NO");
    }
}

void RemoteApplication::taskDisplayUpdate(CommonAppState& state) {
    if (staticApplication) {
        staticApplication->uiManager.tick();
    }
}

void RemoteApplication::taskLoRaUpdate(CommonAppState& state) {
    if (staticServices && staticServices->loraComm) {
        staticServices->loraComm->tick(state.nowMs);
        Radio.IrqProcess();
        if (staticApplication && staticApplication->headerStatus) {
            staticApplication->headerStatus->setLoraStatus(staticServices->loraComm->isConnected(),
                                                          staticServices->loraComm->getLastRssiDbm());
        }
    }
}

void RemoteApplication::taskAnalogRead(CommonAppState& state) {
    // Read analog sensor - cast to access remote-specific fields
    RemoteAppState& remoteState = static_cast<RemoteAppState&>(state);

    // Guard: skip if analog sensor disabled or pin not configured
    if (!staticConfig || !staticConfig->enableAnalogSensor || staticConfig->analogInputPin == 0xFF) {
        return;
    }

    // Use configured analog pin from config
    int raw = 0;
    if (staticConfig->useCalibratedAdc) {
        // Use calibrated millivolt reading for better accuracy
        raw = analogReadMilliVolts(staticConfig->analogInputPin);
        remoteState.analogVoltage = (float)raw / 1000.0f; // Convert mV to V
    } else {
        // Use raw ADC reading
        raw = analogRead(staticConfig->analogInputPin);
        remoteState.analogVoltage = convertAdcToVolts(raw);
    }
    remoteState.analogRaw = raw;

    LOGI("remote", "Analog: raw=%d, voltage=%.3fV", raw, remoteState.analogVoltage);
}

void RemoteApplication::taskTelemetryReport(CommonAppState& state) {
    // Cast to access remote-specific fields
    RemoteAppState& remoteState = static_cast<RemoteAppState&>(state);

    // Check if it's time to report
    if ((int32_t)(millis() - remoteState.nextReportDueMs) < 0) {
        return;
    }

    // Send telemetry as compact JSON for MQTT-friendly consumption (no ACK to simplify)
    if (staticServices && staticServices->loraComm) {
        char buf[64];
        int n = snprintf(buf, sizeof(buf), "{\"id\":\"%s\",\"raw\":%d,\"voltage\":%.3f}",
                        REMOTE_DEVICE_ID, remoteState.analogRaw, remoteState.analogVoltage);

        if (n > 0) {
            staticServices->loraComm->sendData(1, (const uint8_t*)buf,
                                          (uint8_t)min(n, (int)sizeof(buf) - 1), false);
            LOGI("remote", "Telemetry sent: %s", buf);
        }
    }

    // Schedule next report with jitter
    const int32_t jitter = (int32_t)((int32_t)staticConfig->telemetryReportIntervalMs / 5);
    const int32_t delta = (int32_t)staticConfig->telemetryReportIntervalMs + (int32_t)random(-jitter, jitter + 1);
    remoteState.nextReportDueMs = millis() + (uint32_t)max(100, (int)delta);
}

float RemoteApplication::convertAdcToVolts(int adcRaw) {
    const float scale = 1.0f / 4095.0f;
    return (float)adcRaw * scale * 3.30f; // 3.3V reference
}

// Sensor task implementation
void RemoteApplication::taskSensorUpdate(CommonAppState& state) {
    if (!staticApplication) return;

    // Update sensor manager with current time
    staticApplication->sensorManager.update(state.nowMs);
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
