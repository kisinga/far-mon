#include "svc_ui.h"
#include "logo.cpp"

UiService::UiService(IDisplayHal& displayHal)
    : _displayHal(displayHal),
      _screenLayout(displayHal) {
}

void UiService::init() {
    _splashStartedMs = millis();
    _state = UIState::Splash;
    drawSplashScreen();
}

void UiService::tick() {
    switch (_state) {
        case UIState::Splash:
            drawSplashScreen(); // Redraw every tick
            if (millis() - _splashStartedMs > SPLASH_DURATION_MS) {
                _state = UIState::Home;
            }
            break;

        case UIState::Home:
            // Clear the display
            _displayHal.clear();

            // The ScreenLayout needs to be initialized with elements.
            // This should happen in a setup method or be driven by app state.
            _screenLayout.draw();

            // Display the buffer
            _displayHal.display();
            break;
    }
}

void UiService::drawSplashScreen() {
    _displayHal.clear();
    // TODO: Fix this. The HAL doesn't have a drawXbm method.
    _displayHal.drawXbm(32, 0, 64, 64, logo_bits);
    _displayHal.display();
}
