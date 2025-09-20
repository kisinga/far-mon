#pragma once

#include "UIElement.h"
#include <Arduino.h>

class HeaderStatusElement : public UIElement {
public:
    enum class Mode {
        Lora,
        Wifi,
        PeerCount
    };

    HeaderStatusElement();
    void setMode(Mode mode);
    void setLoraStatus(bool connected, int16_t rssi);
    void setWifiStatus(bool connected, int8_t signalStrength);
    void setPeerCount(uint16_t count);
    void draw(SSD1306Wire& display, int x, int y, int w, int h) override;

private:
    void drawLoraSignal(SSD1306Wire& d);
    void drawWifiStatus(SSD1306Wire& d);
    void drawPeerCount(SSD1306Wire& d);

    Mode _mode = Mode::Lora;

    // State for all modes
    bool _loraConnected = false;
    int16_t _loraRssi = -127;
    bool _wifiConnected = false;
    int8_t _wifiSignalStrength = -1;
    uint16_t _peerCount = 0;
};
