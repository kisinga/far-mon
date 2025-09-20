#pragma once

#include "display.h"
#include "ScreenLayout.h"
#include "logo.h" // For splash screen

class UIManager {
public:
    enum class UIState {
        Splash,
        Home
    };

    explicit UIManager(OledDisplay& oled);

    void init();
    void tick();

    ScreenLayout& getLayout() { return _screenLayout; }

private:
    void drawSplashScreen();

    OledDisplay& _oled;
    ScreenLayout _screenLayout;
    UIState _state = UIState::Splash;
    uint32_t _splashStartedMs = 0;
    static const uint32_t SPLASH_DURATION_MS = 1200;
};
