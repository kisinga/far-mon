// Remote Application - Device-specific implementation for remote/slave node
// Uses the common application framework with remote-specific behavior

// Disabled legacy header to avoid conflicts with simplified `remote.ino` implementation
#if 0
#pragma once

#include "../lib/common_app.h"
#include "../lib/device_config.h"

// Remote-specific state
struct RemoteAppState : CommonAppState {
    int analogRaw = 0;
    float analogVoltage = 0.0f;
    uint32_t nextReportDueMs = 0;
};

// Remote Application class
class RemoteApplication : public CommonApplication {
public:
    RemoteApplication();

protected:
    // Device-specific implementations
    void setupDeviceConfig() override;
    void setupDeviceSpecific() override;
    void registerDeviceTasks() override;

private:
    RemoteConfig config;
    RemoteAppState remoteState;

    // Device-specific methods
    void setupAnalogSensor();
    void setupDisplayProviders();
    void readAnalogSensor();
    void sendTelemetryReport();

    // Utility methods
    float convertAdcToVolts(int adcRaw) const;

public:
    // LoRa callbacks (similar to original)
    static void onLoraData(uint8_t src, const uint8_t* payload, uint8_t len);
    static void onLoraAck(uint8_t src, uint16_t msgId);

private:
};
#endif
