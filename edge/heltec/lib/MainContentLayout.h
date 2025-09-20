#pragma once

#include "Layout.h"
#include "UIElement.h"
#include <memory>

class MainContentLayout : public Layout {
public:
    explicit MainContentLayout(SSD1306Wire& display);
    void draw() override;
    void setLeft(std::unique_ptr<UIElement> element);
    void setRight(std::unique_ptr<UIElement> element);
    void setLeftColumnWidth(int16_t width);

private:
    std::unique_ptr<UIElement> _left;
    std::unique_ptr<UIElement> _right;
    int16_t _leftColWidth = -1; // -1 means use default percentage
};
