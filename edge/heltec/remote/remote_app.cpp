#include "remote_app.h"

// Includes from remote.ino
#include "lib/core_config.h"
#include "lib/core_system.h"
#include "lib/core_scheduler.h"
#include "lib/core_logger.h"
#include "lib/hal_display.h"
#include "lib/hal_lora.h"
#include "lib/hal_wifi.h"
#include "lib/hal_battery.h"
#include "lib/hal_persistence.h" // <-- Add include
#include "lib/svc_ui.h"
#include "lib/svc_comms.h"
#include "lib/svc_battery.h"
#include "lib/svc_wifi.h"
#include "lib/svc_lora.h"

#include "remote_sensor_config.h"
#include "config.h"
#include "sensor_interface.hpp"
#include "sensor_implementations.hpp"

#include "lib/ui_battery_icon_element.h"
#include "lib/ui_header_status_element.h"
#include "lib/ui_main_content_layout.h"
#include "lib/ui_screen_layout.h"
#include "lib/ui_top_bar_layout.h"
#include "lib/ui_text_element.h"
#include "lib/ui_icon_element.h"


class RemoteApplicationImpl {
public:
    RemoteApplicationImpl();
    void initialize();
    void run();

private:
    // Configuration must be initialized first
    RemoteConfig config;
    RemoteSensorConfig sensorConfig;

    CoreSystem coreSystem;
    CoreScheduler scheduler;
    CommonAppState appState;


    std::unique_ptr<IDisplayHal> displayHal;
    std::unique_ptr<ILoRaHal> loraHal;
    std::unique_ptr<IWifiHal> wifiHal;
    std::unique_ptr<IBatteryHal> batteryHal;
    std::unique_ptr<IPersistenceHal> persistenceHal; // <-- Add member

    std::unique_ptr<UiService> uiService;
    std::unique_ptr<CommsService> commsService;
    std::unique_ptr<IBatteryService> batteryService;
    std::unique_ptr<IWifiService> wifiService;
    std::unique_ptr<ILoRaService> loraService;

    SensorManager sensorManager;
    std::unique_ptr<LoRaBatchTransmitter> sensorTransmitter;
    std::shared_ptr<YFS201WaterFlowSensor> waterFlowSensor; // <-- Add member

    // UI Elements must be stored to manage their lifetime
    std::vector<std::shared_ptr<UIElement>> uiElements;
    // Pointers for easy access to specific elements
    std::shared_ptr<TextElement> idElement;
    std::shared_ptr<HeaderStatusElement> loraStatusElement;
    std::shared_ptr<BatteryIconElement> batteryElement;
    std::shared_ptr<TextElement> statusTextElement;

    uint32_t _errorCount = 0;
    uint32_t _lastResetMs = 0;

    // Application-level message statistics
    struct LoRaMessageStats {
        uint32_t successful = 0;
        uint32_t recovered = 0;
        uint32_t dropped = 0;
    } loraStats;

    uint32_t lastSuccessfulAckMs = 0;

    void onLoraAckReceived(uint8_t srcId, uint16_t messageId, uint8_t attempts);
    void onLoraMessageDropped(uint16_t messageId, uint8_t attempts);
    void onLoraDataReceived(uint8_t srcId, const uint8_t *payload, uint8_t length);
    static void staticOnAckReceived(uint8_t srcId, uint16_t messageId, uint8_t attempts);
    static void staticOnMessageDropped(uint16_t messageId, uint8_t attempts);
    static void staticOnLoraDataReceived(uint8_t srcId, const uint8_t *payload, uint8_t length);
    static RemoteApplicationImpl* callbackInstance;

    void setupUi();
    void setupSensors();
};

RemoteApplicationImpl* RemoteApplicationImpl::callbackInstance = nullptr;

RemoteApplicationImpl::RemoteApplicationImpl() :
    config(buildRemoteConfig()),
    sensorConfig(buildRemoteSensorConfig()) {
    callbackInstance = this;
}

void RemoteApplicationImpl::initialize() {
    // config and sensorConfig are now initialized in the constructor
    
    // ---
    // Critical: HALs must be created FIRST
    // ---
    persistenceHal = std::make_unique<FlashPersistenceHal>();

    coreSystem.init(config);

    if (config.globalDebugMode) {
        Logger::setLevel(Logger::Level::Debug);
        LOGD("System", "Debug mode is ON. Log level set to DEBUG.");
    }

    // Load persistent state
    persistenceHal->begin("app_state");
    _errorCount = persistenceHal->loadU32("errorCount", 0);
    _lastResetMs = persistenceHal->loadU32("lastResetMs", 0);
    persistenceHal->end();

    LOGI("Remote", "Creating other HALs");
    displayHal = std::make_unique<OledDisplayHal>();
    loraHal = std::make_unique<LoRaCommHal>();
    loraHal->setVerbose(config.communication.usb.verboseLogging);
    batteryHal = std::make_unique<BatteryMonitorHal>(config.battery);

    LOGI("Remote", "Creating services");
    uiService = std::make_unique<UiService>(*displayHal);
    commsService = std::make_unique<CommsService>();
    commsService->setLoraHal(loraHal.get());
    batteryService = std::make_unique<BatteryService>(*batteryHal);
    loraService = std::make_unique<LoRaService>(*loraHal);

    // Only create WiFi components if WiFi is enabled
    if (config.communication.wifi.enableWifi) {
        LOGI("Remote", "WiFi enabled, creating WiFi components");
        WifiManager::Config wifiConfig{
            .ssid = config.communication.wifi.ssid,
            .password = config.communication.wifi.password
        };
        wifiHal = std::make_unique<WifiManagerHal>(wifiConfig);
        commsService->setWifiHal(wifiHal.get());
        wifiService = std::make_unique<WifiService>(*wifiHal);
    } else {
        LOGI("Remote", "WiFi disabled, skipping WiFi components");
    }

    LOGI("Remote", "Beginning hardware initialization");
    displayHal->begin();
    LOGI("Remote", "Display initialized");

    loraHal->begin(ILoRaHal::Mode::Slave, config.deviceId);
    LOGI("Remote", "LoRa initialized");
    loraHal->setOnAckReceived(&RemoteApplicationImpl::staticOnAckReceived);
    loraHal->setOnMessageDropped(&RemoteApplicationImpl::staticOnMessageDropped);
    loraHal->setOnDataReceived(&RemoteApplicationImpl::staticOnLoraDataReceived);
    loraHal->setMasterNodeId(config.masterNodeId);
    loraHal->setPeerTimeout(config.peerTimeoutMs);
    LOGI("Remote", "Sending registration frame...");
    loraHal->sendData(config.masterNodeId, nullptr, 0, true);

    // Only begin WiFi if enabled
    if (config.communication.wifi.enableWifi && wifiHal) {
        wifiHal->begin();
        LOGI("Remote", "WiFi initialized");
    }

    uiService->init(); // Show splash screen
    LOGI("Remote", "UI service initialized");

    setupUi();
    LOGI("Remote", "UI setup complete");

    setupSensors();
    LOGI("Remote", "Sensors setup complete");

    // Perform an initial sensor read and telemetry transmission
    if (sensorConfig.enableSensorSystem) {
        LOGI("Remote", "Performing initial sensor reading and telemetry transmission...");
        sensorManager.update(millis());
    }

    LOGI("Remote", "Registering scheduler tasks");
    scheduler.registerTask("heartbeat", [this](CommonAppState& state){
        state.heartbeatOn = !state.heartbeatOn;
    }, config.heartbeatIntervalMs);
    scheduler.registerTask("battery", [this](CommonAppState& state){
        batteryService->update(state.nowMs);
    }, 1000);
    if (sensorConfig.enableSensorSystem && sensorConfig.waterFlowConfig.enabled) {
        scheduler.registerTask("persistence", [this](CommonAppState& state){
            if (waterFlowSensor) {
                waterFlowSensor->saveTotalVolume();
            }
        }, 60000); // Save volume every minute
    }
    scheduler.registerTask("display", [this](CommonAppState& state){
        uiService->tick();
    }, config.displayUpdateIntervalMs);
    scheduler.registerTask("lora", [this](CommonAppState& state){
        loraService->update(state.nowMs);
        Radio.IrqProcess(); // Handle radio interrupts
        // Update UI elements with new state
        auto connectionState = loraService->getConnectionState();
        bool isConnected = (connectionState == ILoRaService::ConnectionState::Connected);
        loraStatusElement->setLoraStatus(isConnected, loraService->getLastRssiDbm());
        batteryElement->setStatus(batteryService->getBatteryPercent(), batteryService->isCharging());

        // Update main status text with connection and error info
        if (statusTextElement) {
            char statusStr[32];
            const char* connStr = isConnected ? "Online" : "Offline";
            snprintf(statusStr, sizeof(statusStr), "%s\nErrors: %u", connStr, _errorCount);
            statusTextElement->setText(statusStr);
        }
    }, 50);
    if (config.globalDebugMode) {
        scheduler.registerTask("interrupt_debug", [](CommonAppState& state){
            if (YFS201WaterFlowSensor::getAndClearInterruptFlag()) {
                LOGD("Interrupt", "Water flow pulse detected!");
            }
        }, 10); // High-frequency check
    }
    if (sensorConfig.enableSensorSystem) {
        // This task reads sensors and fills the transmitter's buffer
        scheduler.registerTask("sensors", [this](CommonAppState& state){
              sensorManager.update(state.nowMs);
              if (sensorTransmitter) {
                  // Manually add application-level data to the batch
                  sensorTransmitter->addReading({"err_count", (float)_errorCount, state.nowMs});
                  uint32_t timeSinceResetSec = (state.nowMs - _lastResetMs) / 1000;
                  sensorTransmitter->addReading({"tsr", (float)timeSinceResetSec, state.nowMs});
              }
        }, config.telemetryReportIntervalMs);

        // This task attempts to transmit the buffer
        scheduler.registerTask("lora_tx", [this](CommonAppState& state){
            if (loraService->isConnected() && sensorTransmitter) {
                sensorTransmitter->update(state.nowMs);
            }
        }, 1000); // Attempt to transmit every second
    }
    scheduler.registerTask("lora_watchdog", [this](CommonAppState& state){
        if (state.nowMs - lastSuccessfulAckMs > config.maxQuietTimeMs) {
            LOGW("Remote", "Watchdog: No ACK received recently, forcing reconnect.");
            loraService->forceReconnect();
            // Reset the timer to prevent spamming keep-alives before the first one gets a chance to be ACKed
            lastSuccessfulAckMs = state.nowMs;
        }
    }, 30000); // Check every 30 seconds


    if (config.communication.wifi.enableWifi && wifiService) {
        scheduler.registerTask("wifi", [this](CommonAppState& state){
            wifiService->update(state.nowMs);
        }, config.communication.wifi.statusCheckIntervalMs);
    }

    LOGI("Remote", "Starting scheduler");
    scheduler.start(appState);
    LOGI("Remote", "Scheduler started, initialization complete");
}

void RemoteApplicationImpl::setupUi() {
    auto& layout = uiService->getLayout();
    auto& topBar = layout.getTopBar();
    auto& mainContent = layout.getMainContent();

    // -- Top Bar (Remote) --
    // [Device ID] [Battery] [Empty] [LoRa Status as centered icon]
    idElement = std::make_shared<TextElement>();
    idElement->setText(String("ID: ") + String(config.deviceId, HEX));
    uiElements.push_back(idElement);
    topBar.setColumn(TopBarColumn::DeviceId, idElement.get());

    batteryElement = std::make_shared<BatteryIconElement>();
    uiElements.push_back(batteryElement);
    topBar.setColumn(TopBarColumn::Battery, batteryElement.get());

    // LoRa status as centered icon in Network column (same position as peer count on relay)
    loraStatusElement = std::make_shared<HeaderStatusElement>();
    loraStatusElement->setMode(HeaderStatusElement::Mode::Lora);
    uiElements.push_back(loraStatusElement);
    topBar.setColumn(TopBarColumn::Network, loraStatusElement.get());

    // -- Main Content --
    // [Small Logo] [Status Text]
    mainContent.setLeftColumnWidth(logo_small_width + 8); // logo width + more margin
    auto mainLogoElement = std::make_shared<IconElement>(logo_small_bits, logo_small_width, logo_small_height);
    uiElements.push_back(mainLogoElement);
    mainContent.setLeft(mainLogoElement.get());

    statusTextElement = std::make_shared<TextElement>("Ready");
    uiElements.push_back(statusTextElement);
    mainContent.setRight(statusTextElement.get());
}

void RemoteApplicationImpl::setupSensors() {
    if (!sensorConfig.enableSensorSystem) {
        return;
    }

    auto transmitter = std::make_unique<LoRaBatchTransmitter>(loraHal.get(), config.deviceId);
    sensorManager.setTransmitter(transmitter.get());
    sensorTransmitter = std::move(transmitter);

    // --- Sensor Creation ---
    // Create all potential sensors and let them manage their own enabled state.

    auto batterySensor = SensorFactory::createBatteryMonitorSensor(
        batteryService.get(),
        sensorConfig.batteryConfig.enabled
    );
    sensorManager.addSensor(batterySensor);

    waterFlowSensor = SensorFactory::createYFS201WaterFlowSensor(
        sensorConfig.pins.waterFlow,
        sensorConfig.waterFlowConfig.enabled,
        persistenceHal.get(),
        "water_meter"
    );
    sensorManager.addSensor(waterFlowSensor);
}

void RemoteApplicationImpl::run() {
    // This run loop is for high-frequency, non-blocking tasks.
    // The main application logic is handled by the scheduler.

    // A small delay to prevent this loop from starving other tasks if they
    // were ever added to the main Arduino loop.
    delay(1);
}

void RemoteApplicationImpl::onLoraAckReceived(uint8_t srcId, uint16_t messageId, uint8_t attempts) {
    LOGI("Remote", "ACK received from %u for msgId %u after %u attempts", srcId, messageId, attempts);
    if (attempts <= 1) {
        loraStats.successful++;
    } else {
        loraStats.recovered++;
    }
    lastSuccessfulAckMs = millis();
}

void RemoteApplicationImpl::onLoraMessageDropped(uint16_t messageId, uint8_t attempts) {
    LOGW("Remote", "Message %u dropped after %u attempts", messageId, attempts);
    loraStats.dropped++;
    _errorCount++;
    persistenceHal->begin("app_state");
    persistenceHal->saveU32("errorCount", _errorCount);
    persistenceHal->end();
}

void RemoteApplicationImpl::onLoraDataReceived(uint8_t srcId, const uint8_t *payload, uint8_t length) {
    // For commands, we'll use a simple protocol where the first byte of the raw
    // LoRa payload is the CommandType.
    if (length >= 1) {
        Messaging::CommandType cmdType = (Messaging::CommandType)payload[0];
        if (cmdType == Messaging::CommandType::ResetWaterVolume) {
            LOGI("Remote", "Received ResetWaterVolume command from master.");
            if (waterFlowSensor) {
                waterFlowSensor->resetTotalVolume();
            }
            if (loraHal) {
                loraHal->resetCounters();
            }
            Messaging::Message::resetSequenceId();

            // Reset error counter
            _errorCount = 0;
            _lastResetMs = millis(); // Record the time of reset
            persistenceHal->begin("app_state");
            persistenceHal->saveU32("errorCount", _errorCount);
            persistenceHal->saveU32("lastResetMs", _lastResetMs);
            persistenceHal->end();
        }
    }
}

void RemoteApplicationImpl::staticOnAckReceived(uint8_t srcId, uint16_t messageId, uint8_t attempts) {
    if (callbackInstance) {
        callbackInstance->onLoraAckReceived(srcId, messageId, attempts);
    }
}

void RemoteApplicationImpl::staticOnMessageDropped(uint16_t messageId, uint8_t attempts) {
    if (callbackInstance) {
        callbackInstance->onLoraMessageDropped(messageId, attempts);
    }
}

void RemoteApplicationImpl::staticOnLoraDataReceived(uint8_t srcId, const uint8_t *payload, uint8_t length) {
    if (callbackInstance) {
        callbackInstance->onLoraDataReceived(srcId, payload, length);
    }
}

// PIMPL Implementation
RemoteApplication::RemoteApplication() : impl(new RemoteApplicationImpl()) {}
RemoteApplication::~RemoteApplication() { delete impl; }
void RemoteApplication::initialize() { impl->initialize(); }
void RemoteApplication::run() { impl->run(); }

// Force the Arduino build system to compile these implementation files
#include "lib/core_config.cpp"
#include "lib/core_system.cpp"
#include "lib/core_scheduler.cpp"
#include "lib/svc_ui.cpp"
#include "lib/svc_comms.cpp"
#include "lib/svc_battery.cpp"
#include "lib/svc_wifi.cpp"
#include "lib/svc_lora.cpp"
#include "lib/ui_battery_icon_element.cpp"
#include "lib/ui_header_status_element.cpp"
#include "lib/ui_main_content_layout.cpp"
#include "lib/ui_screen_layout.cpp"
#include "lib/ui_top_bar_layout.cpp"
#include "sensor_implementations.hpp"
