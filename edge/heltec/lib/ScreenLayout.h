#pragma once

#include "Layout.h"
#include "TopBarLayout.h"
#include "MainContentLayout.h"

class ScreenLayout : public Layout {
public:
    explicit ScreenLayout(SSD1306Wire& display);
    void draw() override;

    TopBarLayout& getTopBar() { return _topBar; }
    MainContentLayout& getMainContent() { return _mainContent; }

private:
    TopBarLayout _topBar;
    MainContentLayout _mainContent;
};
