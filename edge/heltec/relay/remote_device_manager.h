#pragma once

#include "lib/hal_lora.h"
#include "lib/hal_persistence.h"
#include "lib/common_message_types.h"
#include <vector>
#include <map>
#include "lib/telemetry_keys.h"

// Represents the state of a single remote device
struct RemoteDeviceState {
    uint8_t deviceId = 0;
    uint32_t lastResetMs = 0;
    float dailyVolumeLiters = 0.0f;
    uint32_t errorCount = 0;
    uint32_t timeSinceResetSec = 0; // Time since the remote's last reset
    unsigned long lastMessageMs = 0;
    uint32_t lastTsrSec = 0;
    bool needsSave = false;
};

class RemoteDeviceManager {
public:
    RemoteDeviceManager(ILoRaHal* loraHal, IPersistenceHal* persistence);

    void begin();
    void update(uint32_t nowMs);
    void handleTelemetry(uint8_t srcId, const std::string& payload);

private:
    void loadAllStates();
    void saveState(RemoteDeviceState& state);
    void sendResetCommand(uint8_t deviceId);

    RemoteDeviceState* getOrCreateDevice(uint8_t deviceId);

    ILoRaHal* _loraHal;
    IPersistenceHal* _persistence;
    std::map<uint8_t, RemoteDeviceState> _devices;

    const uint32_t RESET_INTERVAL_MS = 24 * 60 * 60 * 1000; // 24 hours
};

RemoteDeviceManager::RemoteDeviceManager(ILoRaHal* loraHal, IPersistenceHal* persistence)
    : _loraHal(loraHal), _persistence(persistence) {}

void RemoteDeviceManager::begin() {
    loadAllStates();
}

void RemoteDeviceManager::update(uint32_t nowMs) {
    for (auto& pair : _devices) {
        RemoteDeviceState& device = pair.second;
        if (nowMs - device.lastResetMs > RESET_INTERVAL_MS) {
            LOGI("DeviceManager", "Device %u has reached 24-hour reset period. Resetting.", device.deviceId);
            
            // Log the final daily total before resetting (e.g., to MQTT or Serial)
            LOGI("DeviceManager", "Final daily volume for device %u: %.2f L", device.deviceId, device.dailyVolumeLiters);

            sendResetCommand(device.deviceId);
            device.dailyVolumeLiters = 0.0f;
            device.lastResetMs = nowMs;
            device.errorCount = 0; // Also reset the error count
            device.needsSave = true;
        }

        // Persist any changes to the device state
        if (device.needsSave) {
            saveState(device);
        }
    }
}

void RemoteDeviceManager::handleTelemetry(uint8_t srcId, const std::string& payload) {
    unsigned long nowMs = millis();
    RemoteDeviceState* device = getOrCreateDevice(srcId);
    if (!device) return;

    device->lastMessageMs = nowMs;

    size_t start = 0;
    while (start < payload.length()) {
        size_t end = payload.find(',', start);
        if (end == std::string::npos) end = payload.length();
        
        std::string pair_str = payload.substr(start, end - start);
        size_t colon_pos = pair_str.find(':');

        if (colon_pos != std::string::npos) {
            std::string key = pair_str.substr(0, colon_pos);
            std::string value_str = pair_str.substr(colon_pos + 1);

            if (key == TelemetryKeys::TotalVolume) {
                if (value_str != "nan") device->dailyVolumeLiters = std::stof(value_str);
            } else if (key == TelemetryKeys::ErrorCount) {
                if (value_str != "nan") device->errorCount = std::stoul(value_str);
            } else if (key == TelemetryKeys::TimeSinceReset) {
                if (value_str != "nan") device->timeSinceResetSec = std::stoul(value_str);
            } else if (key == TelemetryKeys::PulseDelta) {
                if (value_str != "nan" && device->lastTsrSec > 0) {
                    uint32_t time_delta_sec = device->timeSinceResetSec - device->lastTsrSec;
                    uint16_t pulse_delta = std::stoul(value_str);
                    if (time_delta_sec > 0) {
                        float frequency = (float)pulse_delta / time_delta_sec;
                        float flow_rate_lpm = (frequency * 60.0f) / 450.0f; // Using hardcoded const for now
                        LOGD("DeviceManager", "Device %u flow rate: %.2f L/min (from %u pulses over %u s)", srcId, flow_rate_lpm, pulse_delta, time_delta_sec);
                    }
                }
            }
        }
        start = end + 1;
    }
    device->lastTsrSec = device->timeSinceResetSec;
    device->needsSave = true; // Mark for save on next update cycle
}

RemoteDeviceState* RemoteDeviceManager::getOrCreateDevice(uint8_t deviceId) {
    auto it = _devices.find(deviceId);
    if (it != _devices.end()) {
        return &it->second;
    }

    // Create a new device
    LOGI("DeviceManager", "First time seeing device %u. Creating new state.", deviceId);
    RemoteDeviceState newState;
    newState.deviceId = deviceId;
    newState.lastResetMs = millis(); // Start the timer now
    newState.lastMessageMs = millis();
    newState.needsSave = true;
    _devices[deviceId] = newState;
    return &_devices[deviceId];
}

void RemoteDeviceManager::loadAllStates() {
    // Persistence for the manager itself, to know which devices to load
    _persistence->begin("dev_manager");
    std::string device_list_str = _persistence->loadString("device_list", "");
    _persistence->end();

    // Parse the device list
    std::vector<uint8_t> device_ids;
    size_t start = 0;
    while (start < device_list_str.length()) {
        size_t end = device_list_str.find(',', start);
        if (end == std::string::npos) end = device_list_str.length();
        device_ids.push_back(std::stoi(device_list_str.substr(start, end - start)));
        start = end + 1;
    }

    // Load each device's state
    for (uint8_t id : device_ids) {
        char ns[16];
        snprintf(ns, sizeof(ns), "dev_%u", id);
        _persistence->begin(ns);
        RemoteDeviceState state;
        state.deviceId = id;
        state.lastResetMs = _persistence->loadU32("lastReset", millis());
        state.dailyVolumeLiters = _persistence->loadFloat("dailyVol");
        state.errorCount = _persistence->loadU32("errorCount", 0);
        state.lastMessageMs = millis();
        state.lastTsrSec = _persistence->loadU32("lastTsr", 0);
        _persistence->end();
        _devices[id] = state;
        LOGI("DeviceManager", "Loaded state for device %u", id);
    }
}

void RemoteDeviceManager::saveState(RemoteDeviceState& state) {
    char ns[16];
    snprintf(ns, sizeof(ns), "dev_%u", state.deviceId);
    _persistence->begin(ns);
    _persistence->saveU32("lastReset", state.lastResetMs);
    _persistence->saveFloat("dailyVol", state.dailyVolumeLiters);
    _persistence->saveU32("errorCount", state.errorCount);
    _persistence->saveU32("lastTsr", state.lastTsrSec);
    _persistence->end();

    // Also update the master list of devices
    std::string device_list_str;
    for (const auto& pair : _devices) {
        device_list_str += std::to_string(pair.first) + ",";
    }
    _persistence->begin("dev_manager");
    _persistence->saveString("device_list", device_list_str);
    _persistence->end();
    
    state.needsSave = false;
    LOGD("DeviceManager", "Saved state for device %u", state.deviceId);
}

void RemoteDeviceManager::sendResetCommand(uint8_t deviceId) {
    if (_loraHal) {
        uint8_t payload[] = { (uint8_t)Messaging::CommandType::ResetWaterVolume };
        _loraHal->sendData(deviceId, payload, sizeof(payload), true); // Send with ACK
        LOGI("DeviceManager", "Sent ResetWaterVolume command to device %u", deviceId);
    }
}
