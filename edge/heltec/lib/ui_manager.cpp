#include "ui_manager.h"

UIManager::UIManager(OledDisplay& oled)
    : _oled(oled),
      _screenLayout(oled.getDisplay()) { // Need a way to get the SSD1306Wire object
}

void UIManager::init() {
    _splashStartedMs = millis();
}

void UIManager::tick() {
    auto& display = _oled.getDisplay();
    display.clear();

    if (_state == UIState::Splash) {
        if (millis() - _splashStartedMs > SPLASH_DURATION_MS) {
            _state = UIState::Home;
            // Fall through to draw home screen immediately
        } else {
            drawSplashScreen();
        }
    }

    if (_state == UIState::Home) {
        _screenLayout.draw();
    }

    display.display();
}

void UIManager::drawSplashScreen() {
    auto& display = _oled.getDisplay();
    const int16_t logoX = (display.width() - logo_width) / 2;
    const int16_t logoY = (display.height() - logo_height) / 2;
    display.drawXbm(logoX, logoY, logo_width, logo_height, logo_bits);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(display.width() / 2, 0, F("Farm"));
}
