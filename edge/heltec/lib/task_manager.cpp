// Task Manager Implementation

#include "task_manager.h"
#include "system_services.h"
#include "logger.h"

// Battery logging macros (ensure unique static timer per callsite)
#define __LOG_UNIQ2(name, line) name##line
#define __LOG_UNIQ(name, line) __LOG_UNIQ2(name, line)
#define LOG_BATTERY_EVERY_MS(interval, ...) \
    do { \
        static uint32_t __LOG_UNIQ(__lastLogMs_, __LINE__) = 0; \
        uint32_t now = millis(); \
        if ((now - __LOG_UNIQ(__lastLogMs_, __LINE__)) >= (interval)) { \
            __LOG_UNIQ(__lastLogMs_, __LINE__) = now; \
            LOGI("batt", __VA_ARGS__); \
        } \
    } while(0)

// Common task implementations
static void taskHeartbeat(CommonAppState& state, IDisplayService& display, const char* deviceTypeStr) {
    state.heartbeatOn = !state.heartbeatOn;

    Logger::debugFor(
        [&display, deviceTypeStr](SSD1306Wire& d, void* ctx) {
            (void)ctx;
            int16_t cx, cy, cw, ch;
            display.getContentArea(cx, cy, cw, ch);
            d.setTextAlignment(TEXT_ALIGN_LEFT);
            d.drawString(cx, cy, deviceTypeStr);
            d.drawString(cx, cy + 14, F("Heartbeat"));
        },
        nullptr,
        nullptr, // No serial renderer
        nullptr,
        600
    );
}

static void taskBatteryMonitor(CommonAppState& state, IBatteryService& battery, IDisplayService& display) {
    // Update battery service
    battery.update(state.nowMs);

    // Read battery status
    uint8_t batteryPercent = battery.getBatteryPercent();
    bool isCharging = battery.isCharging();

    // Log battery status periodically
    if (batteryPercent <= 100) { // Valid reading
        LOG_BATTERY_EVERY_MS(5000, "voltage=%.2fV percent=%u",
                            0.0f, (unsigned)batteryPercent); // TODO: Add voltage reading
    } else {
        LOG_BATTERY_EVERY_MS(10000, "battery read failed");
    }

    // Update display
    display.setBatteryStatus(batteryPercent <= 100, batteryPercent);
    display.setBatteryCharging(isCharging);

    LOG_BATTERY_EVERY_MS(5000, "charging status = %s", isCharging ? "yes" : "no");
}

static void taskDisplayUpdate(CommonAppState& state, IDisplayService& display) {
    // Just tick the display - header provider updates are handled automatically
    display.tick(state.nowMs);
}

static void taskWifiMonitor(CommonAppState& state, IWifiService& wifi) {
    // WiFi service handles its own updates
    wifi.update(state.nowMs);

    // Log WiFi status periodically
    LOG_BATTERY_EVERY_MS(10000, "WiFi status: %s (%d%%)",
                        wifi.isConnected() ? "Connected" : "Disconnected",
                        wifi.getSignalStrengthPercent());
}

static void taskLoRaUpdate(CommonAppState& state, ILoRaService& lora) {
    lora.update(state.nowMs);
}

// Register common tasks based on device configuration
void TaskManager::registerCommonTasks(const DeviceConfig& config, SystemServices& services) {
    // Always register heartbeat task
    if (services.display) {
        const char* deviceTypeStr = (config.deviceType == DeviceType::Relay) ? "Master" : "Slave";
        registerTask("heartbeat",
            [deviceTypeStr, &services](CommonAppState& state) {
                taskHeartbeat(state, *services.display, deviceTypeStr);
            },
            config.heartbeatIntervalMs);
    }

    // Register battery monitoring if display is enabled
    if (config.enableDisplay && services.battery && services.display) {
        registerTask("battery",
            [&services](CommonAppState& state) {
                taskBatteryMonitor(state, *services.battery, *services.display);
            },
            1000); // Battery monitoring interval
    }

    // Register display update task
    if (config.enableDisplay && services.display) {
        registerTask("display",
            [&services](CommonAppState& state) {
                taskDisplayUpdate(state, *services.display);
            },
            config.displayUpdateIntervalMs);
    }

    // Register LoRa update task
    if (services.lora) {
        registerTask("lora",
            [&services](CommonAppState& state) {
                taskLoRaUpdate(state, *services.lora);
            },
            config.loraTaskIntervalMs);
    }

    // Register WiFi monitoring task (will be enabled/disabled based on device type)
    if (services.wifi) {
        registerTask("wifi",
            [&services](CommonAppState& state) {
                taskWifiMonitor(state, *services.wifi);
            },
            100); // WiFi monitoring interval
    }
}
