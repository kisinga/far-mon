// Common Application Framework - Base class for all device applications
// Provides common initialization, lifecycle management, and service orchestration

#pragma once

#include <memory>
#include "device_config.h"
#include "system_services.h"
#include "task_manager.h"
#include "system_init.h"
#include "wifi_config.h"

// Forward declarations
class OledDisplay;
class DebugRouter;
class LoRaComm;
struct SystemObjects;

// Base application class
class CommonApplication {
public:
    virtual ~CommonApplication() = default;

    // Main lifecycle methods
    void initialize();
    void run();

protected:
    // Hooks for device-specific behavior
    virtual void setupDeviceSpecific() = 0;
    virtual void registerDeviceTasks() = 0;
    virtual void setupDeviceConfig() = 0;

    // Access to services for device-specific implementations
    SystemServices& getServices() { return services; }
    TaskManager& getTaskManager() { return taskManager; }
    const DeviceConfig& getConfig() const { return *deviceConfig; }

private:
    // Core components
    std::unique_ptr<DeviceConfig> deviceConfig;
    SystemServices services;
    TaskManager taskManager;
    CommonAppState appState;

    // Global objects (similar to existing pattern)
    OledDisplay oled;
    DebugRouter debugRouter;
    LoRaComm lora;

    // Battery components
    BatteryMonitor::Config batteryConfig;
    BatteryMonitor::BatteryMonitor batteryMonitor{batteryConfig};

    // WiFi components (optional)
    WifiManager wifiManager{wifiConfig};

    // Initialization helpers
    void initializeHardware();
    void initializeServices();
    void initializeTasks();
    void initializeDisplay();
    void verifyRtosOrDie();
};
