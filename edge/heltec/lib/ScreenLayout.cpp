#include "ScreenLayout.h"

ScreenLayout::ScreenLayout(SSD1306Wire& display)
    : Layout(display),
      _topBar(display),
      _mainContent(display) {
}

void ScreenLayout::draw() {
    _topBar.draw();

    // Draw horizontal bar
    const int16_t headerSeparatorY = 10;
    _display.drawHorizontalLine(0, headerSeparatorY, _display.width());

    _mainContent.draw();
}
