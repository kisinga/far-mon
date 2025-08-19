// Header-only minimal OLED display manager for Heltec SSD1306
// - Optional: can be disabled at runtime
// - Supports a default homescreen renderer
// - Supports temporary per-task debug screens with timeout

#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "logo.h"
#include "battery.h"

#ifndef OLED_I2C_ADDR
#define OLED_I2C_ADDR 0x3C
#endif

// Forward declare Heltec power pin if available
#ifdef Vext
static inline void vextOn() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW); // ON
}
static inline void vextOff() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, HIGH); // OFF
}
#else
static inline void vextOn() {}
static inline void vextOff() {}
#endif

// Render callback signature: device provides drawing logic
using RenderCallback = void (*)(SSD1306Wire &display, void *context);

enum class LayoutMode : uint8_t { Half = 0, Full = 1 };
enum class HeaderRightMode : uint8_t { SignalBars = 0, PeerCount = 1 };

class OledDisplay {
 public:
  OledDisplay()
      : display(OLED_I2C_ADDR, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED) {
  }

  explicit OledDisplay(uint8_t i2cAddress)
      : display(i2cAddress, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED) {
  }

  void begin(bool enable) {
    enabled = enable;
    if (!enabled) return;
    // Power on OLED rail
    if (vextPinOverride >= 0) {
      pinMode((uint8_t)vextPinOverride, OUTPUT);
      digitalWrite((uint8_t)vextPinOverride, LOW);
    } else {
      vextOn();
    }
    delay(100);

    // Hard reset the panel if RST is wired
#ifdef RST_OLED
    pinMode(RST_OLED, OUTPUT);
    digitalWrite(RST_OLED, LOW);
    delay(20);
    digitalWrite(RST_OLED, HIGH);
    delay(100);
#endif

    // Ensure I2C is up on the expected pins
    Wire.begin(SDA_OLED, SCL_OLED);

    display.init();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    // Configure non-blocking splash; will render during tick() for splashDurationMs
    splashActive = true;
    splashStartedMs = millis();
  }

  void setI2cClock(uint32_t hz) {
    Wire.setClock(hz);
  }

  void setDeviceId(const char *id) {
    deviceId = id;
  }

  void setHomescreenRenderer(RenderCallback cb, void *ctx) {
    homescreenCb = cb;
    homescreenCtx = ctx;
  }

  void setLayoutMode(LayoutMode mode) { layoutMode = mode; }
  LayoutMode getLayoutMode() const { return layoutMode; }

  void setLoraStatus(bool connected, int16_t rssiDbm) {
    loraStatusValid = true;
    loraConnected = connected;
    loraRssiDbm = rssiDbm;
  }

  void clearLoraStatus() {
    loraStatusValid = false;
  }

  // Battery indicator (0..100%). If not set, icon is hidden.
  void setBatteryStatus(bool valid, uint8_t percent) {
    batteryStatusValid = valid;
    batteryPercent = percent > 100 ? 100 : percent;
  }

  // Optional charging indicator overlay
  void setBatteryCharging(bool charging) { batteryCharging = charging; }

  // Header right area configuration
  void setHeaderRightMode(HeaderRightMode mode) { headerRightMode = mode; }
  void setPeerCount(uint16_t count) { headerPeerCount = count; }

  void getContentArea(int16_t &x, int16_t &y, int16_t &w, int16_t &h) const {
    x = lastContentX;
    y = lastContentY;
    w = lastContentW;
    h = lastContentH;
  }

  // Optional: for boards where Vext macro is unavailable, provide the pin explicitly
  void setVextPinOverride(int8_t pin) { vextPinOverride = pin; }

  // Show a temporary debug screen for durationMs from nowMs
  void showDebug(RenderCallback cb, void *ctx, uint32_t nowMs, uint32_t durationMs) {
    if (!enabled) return;
    debugCb = cb;
    debugCtx = ctx;
    debugUntilMs = nowMs + durationMs;
  }

  // Convenience: show temporary overlay for duration from current millis()
  void showDebugFor(RenderCallback cb, void *ctx, uint32_t durationMs) {
    showDebug(cb, ctx, millis(), durationMs);
  }

  // Clear any active debug overlay immediately; homescreen resumes on next tick
  void clearDebug() {
    debugCb = nullptr;
    debugCtx = nullptr;
    debugUntilMs = 0;
  }

  // Call periodically to update the display. Cheap if disabled.
  void tick(uint32_t nowMs) {
    if (!enabled) return;

    display.clear();

    // Splash screen (non-blocking)
    if (splashActive && (nowMs - splashStartedMs) < splashDurationMs) {
      const int16_t logoX = (display.width() - logo_width) / 2;
      const int16_t logoY = (display.height() - logo_height) / 2;
      display.drawXbm(logoX, logoY, logo_width, logo_height, logo_bits);
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.drawString(display.width() / 2, 0, F("Farm"));
      display.display();
      return;
    } else if (splashActive) {
      splashActive = false;
    }

    // Header: device id (left) + optional battery icon; right area is configurable
    int16_t headerLeftWidth = 0;
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    if (deviceId != nullptr) {
      String idText = String("ID:") + deviceId;
      display.drawString(0, 0, idText);
      headerLeftWidth = display.getStringWidth(idText);
    }
    {
      const int16_t battX = headerLeftWidth + (headerLeftWidth > 0 ? 6 : 0);
      const int16_t battY = 1; // within 10px header
      // When charging, replace bars with outline-only + bolt; otherwise draw bars
      const bool showBars = batteryStatusValid && !batteryCharging;
      Battery::drawIcon(display, battX, battY, 16, 8, showBars ? batteryPercent : 255 /*outline-only*/);
      if (batteryCharging) {
        Battery::drawChargingBolt(display, battX, battY, 16, 8);
      }
    }

    drawHeaderRight(display);

    // Content area
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    layoutAndDrawContent(display, nowMs);

    display.display();
  }

  // Utilities to diagnose I2C screen presence. Call after begin().
  bool probeI2C(uint8_t addr) {
    if (!enabled) return false;
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();
    return (error == 0);
  }

  void i2cScan(Print &out) {
    if (!enabled) return;
    out.println(F("[i2c] scanning..."));
    uint8_t count = 0;
    for (uint8_t address = 1; address < 127; address++) {
      Wire.beginTransmission(address);
      uint8_t error = Wire.endTransmission();
      if (error == 0) {
        out.print(F("[i2c] found 0x"));
        if (address < 16) out.print('0');
        out.println(address, HEX);
        count++;
      }
      delay(2);
    }
    if (count == 0) {
      out.println(F("[i2c] no devices found"));
    }
  }

 private:
  static inline bool timeNotExpired(uint32_t nowMs, uint32_t untilMs) {
    // (now < until) with wrap-around safety
    return (int32_t)(nowMs - untilMs) < 0;
  }

  void drawLoraSignal(SSD1306Wire &d) {
    const int16_t topY = 0;
    const int16_t headerH = 10;
    const int8_t bars = 4;
    const int8_t barWidth = 2;
    const int8_t barGap = 1;
    const int8_t maxBarHeight = headerH - 2; // keep small padding
    const int16_t totalWidth = bars * barWidth + (bars - 1) * barGap;
    int16_t startX = d.width() - totalWidth;
    if (!loraStatusValid) {
      // Draw empty outline bars
      for (int i = 0; i < bars; i++) {
        int16_t x = startX + i * (barWidth + barGap);
        d.drawRect(x, topY + (maxBarHeight - 2), barWidth, 2);
      }
      return;
    }

    uint8_t level = 0;
    if (loraConnected) {
      // Map RSSI [-120..-80+] to 1..4
      int16_t rssi = loraRssiDbm;
      if (rssi < -115) level = 1;
      else if (rssi < -105) level = 2;
      else if (rssi < -95) level = 3;
      else level = 4;
    } else {
      level = 0;
    }

    for (int i = 0; i < bars; i++) {
      int16_t x = startX + i * (barWidth + barGap);
      int8_t h = (int8_t)((i + 1) * maxBarHeight / bars);
      int16_t y = topY + (maxBarHeight - h);
      if (i < level) {
        d.fillRect(x, y, barWidth, h);
      } else {
        d.drawRect(x, y, barWidth, h);
      }
    }
  }

  void drawPeersCount(SSD1306Wire &d) {
    d.setTextAlignment(TEXT_ALIGN_RIGHT);
    d.drawString(d.width(), 0, String("P:") + String((uint32_t)headerPeerCount));
  }

  void drawHeaderRight(SSD1306Wire &d) {
    if (headerRightMode == HeaderRightMode::SignalBars) {
      drawLoraSignal(d);
    } else {
      drawPeersCount(d);
    }
  }

  // Battery drawing handled by Battery::drawIcon

  void layoutAndDrawContent(SSD1306Wire &d, uint32_t nowMs) {
    const int16_t headerSeparatorY = 10;
    d.drawHorizontalLine(0, headerSeparatorY, d.width());

    // Default content area spans full width below header
    int16_t contentX = 0;
    int16_t contentY = headerSeparatorY + 2;
    int16_t contentW = d.width();
    int16_t contentH = d.height() - contentY;

    // Half layout: draw logo on the left and shrink content area
    if (layoutMode == LayoutMode::Half) {
      // Draw provided small logo on the left (homescreen only)
      #if defined(logo_small_width) && defined(logo_small_height)
      const int16_t logoW = logo_small_width;
      const int16_t logoH = logo_small_height;
      d.drawXbm(0, contentY, logoW, logoH, logo_small_bits);
      const int16_t margin = 4;
      contentX = logoW + margin;
      // Constrain content to ~3/4 of remaining width for a lighter look
      int16_t remaining = d.width() - contentX;
      int16_t threeQuarters = (int16_t)((remaining * 3) / 4);
      contentW = threeQuarters > 20 ? threeQuarters : remaining; // ensure minimum width
      #endif
    }

    // Persist last computed area for renderers to query
    lastContentX = contentX;
    lastContentY = contentY;
    lastContentW = contentW;
    lastContentH = contentH;

    if (debugCb != nullptr && timeNotExpired(nowMs, debugUntilMs)) {
      debugCb(d, debugCtx);
    } else if (homescreenCb != nullptr) {
      homescreenCb(d, homescreenCtx);
      // Clear debug state once expired
      if (debugUntilMs != 0 && (int32_t)(nowMs - debugUntilMs) >= 0) {
        debugCb = nullptr;
        debugCtx = nullptr;
        debugUntilMs = 0;
      }
    } else {
      d.drawString(contentX, contentY + 2, F("No homescreen set"));
    }
  }

  bool enabled = false;
  const char *deviceId = nullptr;

  int8_t vextPinOverride = -1; // -1 means use Vext macro if available

  // Splash state
  bool splashActive = false;
  uint32_t splashStartedMs = 0;
  uint32_t splashDurationMs = 1200; // ms

  RenderCallback homescreenCb = nullptr;
  void *homescreenCtx = nullptr;

  RenderCallback debugCb = nullptr;
  void *debugCtx = nullptr;
  uint32_t debugUntilMs = 0;

  // Layout status
  LayoutMode layoutMode = LayoutMode::Half;
  // Last computed content area (for renderers to query)
  int16_t lastContentX = 0;
  int16_t lastContentY = 12;
  int16_t lastContentW = 128;
  int16_t lastContentH = 52;

  // LoRa status for header
  bool loraStatusValid = false;
  bool loraConnected = false;
  int16_t loraRssiDbm = -127;

  // Battery status
  bool batteryStatusValid = false;
  uint8_t batteryPercent = 100;
  bool batteryCharging = false;

  // Header configuration
  HeaderRightMode headerRightMode = HeaderRightMode::SignalBars;
  uint16_t headerPeerCount = 0;

  SSD1306Wire display;
};


