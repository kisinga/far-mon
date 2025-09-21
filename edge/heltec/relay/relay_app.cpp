#include "relay_app.h"
#include <Arduino.h> // For vTaskDelay, pdMS_TO_TICKS

// Includes from relay.ino
#include "lib/core_config.h"
#include "lib/core_system.h"
#include "lib/core_scheduler.h"
#include "lib/core_logger.h"
#include "lib/hal_display.h"
#include "lib/hal_lora.h"
#include "lib/hal_wifi.h"
#include "lib/hal_battery.h"
#include "lib/svc_ui.h"
#include "lib/svc_comms.h"
#include "lib/svc_battery.h"
#include "lib/svc_wifi.h"
#include "lib/svc_lora.h"
#include "lib/ui_text_element.h"
#include "lib/ui_icon_element.h"
#include "lib/ui_battery_icon_element.h"
#include "lib/ui_header_status_element.h"
#include "config.h"
#include <memory>

// Legacy drivers (HAL dependencies)
#include "lib/display.h"
#include "lib/lora_comm.h"
#include "lib/wifi_manager.h"
#include "lib/battery_monitor.h"

class RelayApplicationImpl {
public:
    RelayApplicationImpl();
    void initialize();
    void run();

private:
    // Configuration must be initialized first, so declare it first.
    RelayConfig config;
    
    // Core systems
    CoreSystem coreSystem;
    CoreScheduler scheduler;
    CommonAppState appState;

    // HALs and Services (unique_ptrs are safer)
    std::unique_ptr<IDisplayHal> displayHal;
    std::unique_ptr<ILoRaHal> loraHal;
    std::unique_ptr<IWifiHal> wifiHal;
    std::unique_ptr<IBatteryHal> batteryHal;

    std::unique_ptr<UiService> uiService;
    std::unique_ptr<CommsService> commsService;
    std::unique_ptr<IBatteryService> batteryService;
    std::unique_ptr<IWifiService> wifiService;
    std::unique_ptr<ILoRaService> loraService;

    // UI Elements
    std::vector<std::shared_ptr<UIElement>> uiElements;
    std::shared_ptr<TextElement> statusTextElement;
    std::shared_ptr<BatteryIconElement> batteryElement;
    std::shared_ptr<HeaderStatusElement> peerStatusElement;
    std::shared_ptr<HeaderStatusElement> wifiStatusElement;

    // LoRa message handling
    void onLoraDataReceived(uint8_t srcId, const uint8_t* payload, uint8_t length);
    void onLoraAckReceived(uint8_t srcId, uint16_t messageId);

    // Static callback functions for LoRa HAL
    static void staticOnDataReceived(uint8_t srcId, const uint8_t* payload, uint8_t length);
    static void staticOnAckReceived(uint8_t srcId, uint16_t messageId);

    // Static instance pointer for callbacks
    static RelayApplicationImpl* callbackInstance;

    void setupUi();
};

RelayApplicationImpl::RelayApplicationImpl() :
    config(buildRelayConfig()) {
    // Set the static callback instance for LoRa HAL callbacks
    callbackInstance = this;
}

void RelayApplicationImpl::initialize() {
    // config is now initialized in the constructor
    coreSystem.init(config);
    
    // Create self-contained HALs
    displayHal = std::make_unique<OledDisplayHal>();
    loraHal = std::make_unique<LoRaCommHal>();
    batteryHal = std::make_unique<BatteryMonitorHal>(config.battery);

    // Create services
    uiService = std::make_unique<UiService>(*displayHal);
    commsService = std::make_unique<CommsService>();
    commsService->setLoraHal(loraHal.get());
    batteryService = std::make_unique<BatteryService>(*batteryHal);
    loraService = std::make_unique<LoRaService>(*loraHal);

    // Only create WiFi components if WiFi is enabled
    if (config.communication.wifi.enableWifi) {
        wifiHal = std::make_unique<WifiManagerHal>(WifiManager::Config{
            .ssid = config.communication.wifi.ssid,
            .password = config.communication.wifi.password
        });
        commsService->setWifiHal(wifiHal.get());
        wifiService = std::make_unique<WifiService>(*wifiHal);
    }

    // Configure MQTT if enabled (only if WiFi is also enabled)
    if (config.communication.mqtt.enableMqtt && config.communication.wifi.enableWifi && wifiHal) {
        MqttPublisherConfig mqttConfig;
        mqttConfig.enableMqtt = true;
        mqttConfig.brokerHost = config.communication.mqtt.brokerHost;
        mqttConfig.brokerPort = config.communication.mqtt.brokerPort;
        mqttConfig.clientId = config.communication.mqtt.clientId;
        mqttConfig.username = config.communication.mqtt.username;
        mqttConfig.password = config.communication.mqtt.password;
        mqttConfig.baseTopic = config.communication.mqtt.baseTopic;
        mqttConfig.deviceTopic = config.communication.mqtt.deviceTopic;
        mqttConfig.qos = config.communication.mqtt.qos;
        mqttConfig.retain = config.communication.mqtt.retain;
        wifiHal->setMqttConfig(mqttConfig);
    }
    
    // Begin hardware
    displayHal->begin();
    loraHal->begin(ILoRaHal::Mode::Master, config.deviceId);

    // Only begin WiFi if enabled and WiFi HAL exists
    if (config.communication.wifi.enableWifi && wifiHal) {
        wifiHal->begin();
    }

    // Set up LoRa callbacks
    loraHal->setOnDataReceived(&RelayApplicationImpl::staticOnDataReceived);
    loraHal->setOnAckReceived(&RelayApplicationImpl::staticOnAckReceived);

    uiService->init(); // Show splash screen
    setupUi();

    
    scheduler.registerTask("heartbeat", [this](CommonAppState& state){
        state.heartbeatOn = !state.heartbeatOn;
    }, config.heartbeatIntervalMs);
    
    scheduler.registerTask("battery", [this](CommonAppState& state){
        batteryService->update(state.nowMs);
        if (batteryElement) {
            batteryElement->setStatus(batteryService->getBatteryPercent(), batteryService->isCharging());
        }
    }, 1000);
    
    scheduler.registerTask("display", [this](CommonAppState& state){
        uiService->tick();
    }, config.displayUpdateIntervalMs);
    
    scheduler.registerTask("lora", [this](CommonAppState& state){
        loraService->update(state.nowMs);
        Radio.IrqProcess(); // Handle radio interrupts
        if (peerStatusElement) {
            peerStatusElement->setPeerCount(loraService->getPeerCount());
        }
    }, 50);
    
    if (config.communication.wifi.enableWifi && wifiService) {
        scheduler.registerTask("wifi", [this](CommonAppState& state){
            wifiService->update(state.nowMs);
            if (wifiStatusElement) {
                wifiStatusElement->setWifiStatus(wifiService->isConnected(), wifiService->getSignalStrengthPercent());
            }
        }, config.communication.wifi.statusCheckIntervalMs);
    }
    
    
    scheduler.start(appState);
}

void RelayApplicationImpl::setupUi() {
    auto& layout = uiService->getLayout();
    auto& topBar = layout.getTopBar();
    auto& mainContent = layout.getMainContent();

    // -- Top Bar (Relay) --
    // [Device ID] [Battery] [WiFi Status] [Peer Count as centered icon]
    auto idElement = std::make_shared<TextElement>();
    idElement->setText(String("ID: ") + String(config.deviceId));
    uiElements.push_back(idElement);
    topBar.setColumn(TopBarColumn::DeviceId, idElement.get());

    batteryElement = std::make_shared<BatteryIconElement>();
    uiElements.push_back(batteryElement);
    topBar.setColumn(TopBarColumn::Battery, batteryElement.get());

    // WiFi status in Status column
    wifiStatusElement = std::make_shared<HeaderStatusElement>();
    wifiStatusElement->setMode(HeaderStatusElement::Mode::Wifi);
    // Initialize as disconnected until service updates
    wifiStatusElement->setWifiStatus(false, -1);
    uiElements.push_back(wifiStatusElement);
    topBar.setColumn(TopBarColumn::Status, wifiStatusElement.get());

    // Peer count as centered icon in Network column (same position as LoRa icon on remote)
    peerStatusElement = std::make_shared<HeaderStatusElement>();
    peerStatusElement->setMode(HeaderStatusElement::Mode::PeerCount);
    // Initialize with zero peers
    peerStatusElement->setPeerCount(0);
    uiElements.push_back(peerStatusElement);
    topBar.setColumn(TopBarColumn::Network, peerStatusElement.get());

    // -- Main Content --
    // [Small Logo] [Status Text]
    mainContent.setLeftColumnWidth(logo_small_width + 8); // logo width + more margin
    auto logoElement = std::make_shared<IconElement>(logo_small_bits, logo_small_width, logo_small_height);
    uiElements.push_back(logoElement);
    mainContent.setLeft(logoElement.get());

    statusTextElement = std::make_shared<TextElement>();
    statusTextElement->setText("Ready");
    uiElements.push_back(statusTextElement);
    mainContent.setRight(statusTextElement.get());
}

void RelayApplicationImpl::onLoraDataReceived(uint8_t srcId, const uint8_t* payload, uint8_t length) {
    // Process received LoRa data and forward to MQTT if WiFi is available
    if (config.communication.wifi.enableWifi && wifiHal->isMqttReady()) {
        // Format MQTT topic as "farm/telemetry/remote-{srcId}"
        char topicSuffix[32];
        snprintf(topicSuffix, sizeof(topicSuffix), "remote-%u", srcId);

        // Forward the sensor data to MQTT
        if (!wifiHal->publishMqtt(topicSuffix, payload, length)) {
            LOGW("Relay", "Failed to forward sensor data from device %u to MQTT", srcId);
        } else {
            LOGI("Relay", "Forwarded %u bytes from device %u to MQTT", length, srcId);
        }
    }

    // Update status text to show activity
    if (statusTextElement) {
        statusTextElement->setText(String("RX: ") + length + "b from " + srcId);
    }
}

void RelayApplicationImpl::onLoraAckReceived(uint8_t srcId, uint16_t messageId) {
    // Handle ACK received (for debugging)
    LOGD("Relay", "ACK received from device %u for message %u", srcId, messageId);
}

void RelayApplicationImpl::run() {
    // The scheduler runs tasks in the background. We can just yield.
    vTaskDelay(pdMS_TO_TICKS(1000));
}

// Static callback functions for LoRa HAL
void RelayApplicationImpl::staticOnDataReceived(uint8_t srcId, const uint8_t* payload, uint8_t length) {
    if (callbackInstance) {
        callbackInstance->onLoraDataReceived(srcId, payload, length);
    }
}

void RelayApplicationImpl::staticOnAckReceived(uint8_t srcId, uint16_t messageId) {
    if (callbackInstance) {
        callbackInstance->onLoraAckReceived(srcId, messageId);
    }
}

// Static instance pointer for callbacks
RelayApplicationImpl* RelayApplicationImpl::callbackInstance = nullptr;

// PIMPL Implementation
RelayApplication::RelayApplication() : impl(new RelayApplicationImpl()) {}
RelayApplication::~RelayApplication() { delete impl; }
void RelayApplication::initialize() { impl->initialize(); }
void RelayApplication::run() { impl->run(); }

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
