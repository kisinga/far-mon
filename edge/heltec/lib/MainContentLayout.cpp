#include "MainContentLayout.h"

MainContentLayout::MainContentLayout(SSD1306Wire& display) : Layout(display) {
}

void MainContentLayout::setLeft(std::unique_ptr<UIElement> element) {
    _left = std::move(element);
}

void MainContentLayout::setRight(std::unique_ptr<UIElement> element) {
    _right = std::move(element);
}

void MainContentLayout::setLeftColumnWidth(int16_t width) {
    _leftColWidth = width;
}

void MainContentLayout::draw() {
    const int16_t headerSeparatorY = 10;
    const int16_t contentY = headerSeparatorY + 2;
    const int16_t contentH = _display.height() - contentY;

    const int16_t col1Width = (_leftColWidth > 0) ? _leftColWidth : _display.width() * 0.35;
    const int16_t col2Width = _display.width() - col1Width;
    const int16_t col2X = col1Width;

    if (_left) {
        _left->draw(_display, 0, contentY, col1Width, contentH);
    }
    if (_right) {
        _right->draw(_display, col2X, contentY, col2Width, contentH);
    }
}
