#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include <Arduino.h>
#include <string>
// Forward declaration to avoid heavy dependency in header
class DisplayManager;


enum DeviceState {
    INIT,
    IDLE,
    SENDING,
    RECEIVING,
    ERROR
};

class StateManager {
public:
    StateManager(DisplayManager* displayManager);
    virtual ~StateManager() = default;

    void init();
    void updateState(DeviceState newState);
    DeviceState getCurrentState() const;
    const char* getDeviceId() const;

    virtual void updateDisplay();

protected:
    DisplayManager* _displayManager;
    DeviceState _currentState;
    std::string _deviceId;

    void generateDeviceId();
    const char* stateToString(DeviceState state) const;
};

#endif // STATE_MANAGER_H