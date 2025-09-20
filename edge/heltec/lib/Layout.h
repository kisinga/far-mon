#pragma once

#include "HT_SSD1306Wire.h"

// Base class for layout containers
class Layout {
public:
    explicit Layout(SSD1306Wire& display) : _display(display) {}
    virtual ~Layout() = default;

    virtual void draw() = 0;
    virtual void update() { /* optional */ }

protected:
    SSD1306Wire& _display;
};
