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

// Legacy drivers (HAL dependencies)
#include "lib/display.h"
#include "lib/lora_comm.h"
#include "lib/wifi_manager.h"
#include "lib/battery_monitor.h"

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

    // Legacy drivers
    OledDisplay oled;
    LoRaComm lora;
    WifiManager wifiManager;
    BatteryMonitor::BatteryMonitor batteryMonitor;

    std::unique_ptr<IDisplayHal> displayHal;
    std::unique_ptr<ILoRaHal> loraHal;
    std::unique_ptr<IWifiHal> wifiHal;
    std::unique_ptr<IBatteryHal> batteryHal;

    std::unique_ptr<UiService> uiService;
    std::unique_ptr<CommsService> commsService;
    std::unique_ptr<IBatteryService> batteryService;
    std::unique_ptr<IWifiService> wifiService;
    std::unique_ptr<ILoRaService> loraService;
    
    SensorManager sensorManager;
    std::unique_ptr<LoRaBatchTransmitter> sensorTransmitter;
    
    // UI Elements must be stored to manage their lifetime
    std::vector<std::shared_ptr<UIElement>> uiElements;
    // Pointers for easy access to specific elements
    std::shared_ptr<TextElement> idElement;
    std::shared_ptr<HeaderStatusElement> loraStatusElement;
    std::shared_ptr<BatteryIconElement> batteryElement;
    std::shared_ptr<TextElement> statusTextElement;


    void setupUi();
    void setupSensors();
};

RemoteApplicationImpl::RemoteApplicationImpl() :
    config(buildRemoteConfig()),
    sensorConfig(buildRemoteSensorConfig()),
    wifiManager(WifiManager::Config{
        .ssid = config.communication.wifi.ssid,
        .password = config.communication.wifi.password
    }),
    batteryMonitor(config.battery) {
}

void RemoteApplicationImpl::initialize() {
    // config and sensorConfig are now initialized in the constructor
    coreSystem.init(config);
    displayHal = std::make_unique<OledDisplayHal>();
    loraHal = std::make_unique<LoRaCommHal>();
    wifiHal = std::make_unique<WifiManagerHal>(wifiManager.getConfig());
    batteryHal = std::make_unique<BatteryMonitorHal>(batteryMonitor.getConfig());
    uiService = std::make_unique<UiService>(*displayHal);
    commsService = std::make_unique<CommsService>();
    commsService->setLoraHal(loraHal.get());
    commsService->setWifiHal(wifiHal.get());
    batteryService = std::make_unique<BatteryService>(*batteryHal);
    wifiService = std::make_unique<WifiService>(*wifiHal);
    loraService = std::make_unique<LoRaService>(*loraHal);
    displayHal->begin();
    loraHal->begin(ILoRaHal::Mode::Slave, config.deviceId);
    
    uiService->init(); // Show splash screen

    setupUi();
    setupSensors();
    
    scheduler.registerTask("heartbeat", [this](CommonAppState& state){
        state.heartbeatOn = !state.heartbeatOn;
    }, config.heartbeatIntervalMs);
    scheduler.registerTask("battery", [this](CommonAppState& state){
        batteryService->update(state.nowMs);
    }, 1000);
    scheduler.registerTask("display", [this](CommonAppState& state){
        uiService->tick();
    }, config.displayUpdateIntervalMs);
    scheduler.registerTask("lora", [this](CommonAppState& state){
        loraService->update(state.nowMs);
        Radio.IrqProcess(); // Handle radio interrupts
        // Update UI elements with new state
        loraStatusElement->setLoraStatus(loraService->isConnected(), loraService->getLastRssiDbm());
        batteryElement->setStatus(batteryService->getBatteryPercent(), batteryService->isCharging());
    }, 50);
    /*
    if (sensorConfig.enableSensorSystem) {
        scheduler.registerTask("sensors", [this](CommonAppState& state){
            sensorManager.update(state.nowMs);
        }, 1000);
    }
    */
    
    scheduler.start(appState);
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


    // Configure and add sensors... (this logic is restored from the old remote.ino)
    /*
    if (sensorConfig.ultrasonicConfig.enabled) {
        auto ultrasonic = SensorFactory::createUltrasonicSensor(
            sensorConfig.ultrasonicConfig,
            sensorConfig.pins.ultrasonicTrig,
            sensorConfig.pins.ultrasonicEcho);
        sensorManager.addSensor(std::move(ultrasonic));
    }
    // ... other sensor creation logic would go here
    */
}

void RemoteApplicationImpl::run() {
    delay(1000);
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
