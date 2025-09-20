#include "TopBarLayout.h"

TopBarLayout::TopBarLayout(SSD1306Wire& display) : Layout(display) {
}

void TopBarLayout::setColumn(int index, std::unique_ptr<UIElement> element) {
    if (index >= 0 && index < 4) {
        _columns[index] = std::move(element);
    }
}

void TopBarLayout::draw() {
    const int colWidth = _display.width() / 4;
    for (int i = 0; i < 4; ++i) {
        if (_columns[i]) {
            _columns[i]->draw(_display, i * colWidth, 0, colWidth, 10); // Top 10 pixels for header
        }
    }
}
