#pragma once

#include "Layout.h"
#include "UIElement.h"
#include <memory>

class TopBarLayout : public Layout {
public:
    explicit TopBarLayout(SSD1306Wire& display);
    void draw() override;
    void setColumn(int index, std::unique_ptr<UIElement> element);

private:
    std::unique_ptr<UIElement> _columns[4];
};
