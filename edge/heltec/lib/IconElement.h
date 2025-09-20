#pragma once

#include "UIElement.h"

class IconElement : public UIElement {
public:
    IconElement(int w, int h, const uint8_t* buffer)
        : _w(w), _h(h), _buffer(buffer) {}

    void draw(SSD1306Wire& display, int x, int y, int w, int h) override {
        // Center the icon within the given area
        int iconX = x + (w - _w) / 2;
        int iconY = y + (h - _h) / 2;
        display.drawXbm(iconX, iconY, _w, _h, _buffer);
    }

private:
    int _w, _h;
    const uint8_t* _buffer;
};
