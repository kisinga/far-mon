#pragma once

#include "HT_SSD1306Wire.h"

// Base interface for all drawable UI components
class UIElement {
public:
    virtual ~UIElement() = default;

    // Draw the element within a given bounding box
    virtual void draw(SSD1306Wire& display, int x, int y, int w, int h) = 0;
};
