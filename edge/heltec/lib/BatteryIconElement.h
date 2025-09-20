#pragma once

#include "UIElement.h"

class BatteryIconElement : public UIElement {
public:
    BatteryIconElement();
    void setStatus(uint8_t percent, bool isCharging);
    void draw(SSD1306Wire& display, int x, int y, int w, int h) override;

private:
    void drawBatteryIcon(SSD1306Wire& d, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent);
    void drawChargingBolt(SSD1306Wire& d, int16_t x, int16_t y, int16_t w, int16_t h);

    uint8_t _percent = 100;
    bool _isCharging = false;
    bool _filterInitialized = false;
    float _percentFiltered = 0.0f;
};
