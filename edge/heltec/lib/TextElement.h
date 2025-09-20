#pragma once

#include "UIElement.h"
#include <Arduino.h>

class TextElement : public UIElement {
public:
    explicit TextElement(const String& text) : _text(text) {}

    void setText(const String& text) {
        _text = text;
    }

    void draw(SSD1306Wire& display, int x, int y, int w, int h) override {
        // Simple implementation: draw text at the top-left of the bounding box.
        // Could be extended to support alignment, wrapping, etc.
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.drawString(x, y, _text);
    }

private:
    String _text;
};
