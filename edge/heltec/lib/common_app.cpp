// Common Application Framework Implementation

#include "common_app.h"
#include "display.h"
#include "debug.h"
#include "logger.h"
#include "lora_comm.h"
#include "board_config.h"
#include "wifi_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Initialize the application
void CommonApplication::initialize() {
    // Setup device-specific configuration first
    setupDeviceConfig();

    // Initialize hardware and services
    initializeHardware();
    verifyRtosOrDie();
    initializeServices();
    initializeTasks();
    initializeDisplay();

    // Allow device-specific setup
    setupDeviceSpecific();

    // Register device-specific tasks
    registerDeviceTasks();

    LOGI("app", "Application initialization complete for %s",
         deviceConfig->deviceType == DeviceType::Relay ? "Relay" : "Remote");
}

// Main application loop
void CommonApplication::run() {
    // If RTOS backend is used, the scheduler runs in its own task; keep loop light.
    while (true) {
        taskManager.update(appState);
        delay(1);
    }
}

// Initialize hardware components
void CommonApplication::initializeHardware() {
    // Serial initialization
    Serial.begin(115200);
    delay(200);
    Serial.println();

    // Logger initialization
    Logger::begin(true, &oled, deviceConfig->deviceId);
    Logger::setLevel(Logger::Level::Info);
    Logger::setVerbose(false);
    LOGI("boot", "System starting...");

    // Display initialization
    oled.begin(deviceConfig->enableDisplay);
    oled.setDeviceId(deviceConfig->deviceId);

    // Debug router initialization
    debugRouter.begin(deviceConfig->enableDebug, &oled, deviceConfig->deviceId);

    // Battery configuration (common setup)
    batteryConfig.adcPin = BATTERY_ADC_PIN;
    batteryConfig.ctrlPin = VBAT_CTRL;
    batteryConfig.samples = 12;
    batteryConfig.useHeltecV3Scaling = true;
    batteryConfig.setAttenuationOnFirstRead = true;

    // Configure charge status pin
    #ifdef CHARGE_STATUS_PIN
    static const int8_t kChargeStatusPin = CHARGE_STATUS_PIN;
    #ifdef CHARGE_STATUS_ACTIVE_LOW
    static const bool kChargeActiveLow = (CHARGE_STATUS_ACTIVE_LOW != 0);
    #else
    static const bool kChargeActiveLow = true;
    #endif
    if (kChargeStatusPin >= 0) {
        batteryMonitor.initChargeDetection(kChargeStatusPin, kChargeActiveLow, millis());
        LOGI("batt", "charge status pin=%d init_raw=%d", (int)kChargeStatusPin, (int)digitalRead((uint8_t)kChargeStatusPin));
    }
    #endif

    // I2C/display diagnostics
    if (deviceConfig->enableDisplay) {
        oled.setI2cClock(500000);
        bool found = oled.probeI2C(OLED_I2C_ADDR);
        LOGI("disp", "probe 0x%02X found=%s", OLED_I2C_ADDR, found ? "yes" : "no");
        if (!found) {
            LOGW("disp", "Check Vext power, SDA/SCL pins, or address (0x3C vs 0x3D)");
            oled.i2cScan(Serial);
        }
    }

    // LoRa initialization (determine mode based on device type)
    LoRaComm::Mode loraMode = (deviceConfig->deviceType == DeviceType::Relay) ?
                              LoRaComm::Mode::Master : LoRaComm::Mode::Slave;
    uint8_t selfId = (uint8_t)strtoul(deviceConfig->deviceId, nullptr, 10);
    if (selfId == 0) selfId = 1;

    lora.begin(loraMode, selfId);
    lora.setVerbose(false);
    lora.setLogLevel((uint8_t)Logger::Level::Info);

    LOGI("boot", "RF=%lu Hz tx=%d dBm", (unsigned long)LORA_COMM_RF_FREQUENCY, (int)LORA_COMM_TX_POWER_DBM);
    LOGI("boot", "System initialization complete.");
}

// Initialize service layer
void CommonApplication::initializeServices() {
    services = SystemServices::create(oled, debugRouter, wifiManager, batteryMonitor, lora);
}

// Initialize task management
void CommonApplication::initializeTasks() {
    taskManager.registerCommonTasks(*deviceConfig, services);
    taskManager.start(appState);
}

// Initialize display settings
void CommonApplication::initializeDisplay() {
    if (deviceConfig->enableDisplay) {
        oled.setHomescreenRenderer(nullptr, nullptr); // Will be set by device-specific code
        oled.setHeaderRightMode(HeaderRightMode::SignalBars); // Default
    }
}

void CommonApplication::verifyRtosOrDie() {
    // Ensure FreeRTOS scheduler is running; otherwise log and display fatal error and halt.
    const eTaskState dummy = eRunning; (void)dummy; // ensure header pulled in
    const BaseType_t started = (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
    if (started) return;

    LOGE("rtos", "FATAL: FreeRTOS scheduler not running; halting");
    if (deviceConfig && deviceConfig->enableDisplay) {
        Logger::overlay("RTOS ERROR", "Scheduler not running", millis(), 60000);
    }
    while (true) {
        delay(1000);
    }
}
