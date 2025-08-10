#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <U8g2lib.h>
#include <Wire.h>

// Define the OLED reset pin, if applicable for your board.
// For Heltec WiFi LoRa 32 (V3), it's typically GPIO16.
#define OLED_RST_PIN 16

class DisplayManager {
public:
    DisplayManager();
    void init();
    void showBootLogo();
    void showText(const char* text, int x, int y);
    void updateDisplay(const char* deviceId, const char* status, const char* connectedDevices);
    void clear();

private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
};

#endif // DISPLAY_MANAGER_H