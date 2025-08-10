#include "State.h"
#include "display.h"
#include <esp_system.h>
#include "esp_mac.h"

StateManager::StateManager(DisplayManager* displayManager)
    : _displayManager(displayManager),
      _currentState(INIT) {
}

void StateManager::init() {
    generateDeviceId();
    updateState(IDLE);
}

void StateManager::updateState(DeviceState newState) {
    _currentState = newState;
    Serial.printf("State updated to: %s\n", stateToString(_currentState));
    updateDisplay();
}

DeviceState StateManager::getCurrentState() const {
    return _currentState;
}

const char* StateManager::getDeviceId() const {
    return _deviceId.c_str();
}

void StateManager::generateDeviceId() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char macStr[13];
    sprintf(macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    _deviceId = macStr;
}

void StateManager::updateDisplay() {
    _displayManager->updateDisplay(_deviceId.c_str(), stateToString(_currentState), "N/A");
}

const char* StateManager::stateToString(DeviceState state) const {
    switch (state) {
        case INIT: return "INIT";
        case IDLE: return "IDLE";
        case SENDING: return "SENDING";
        case RECEIVING: return "RECEIVING";
        case ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}