// Header-only minimal OLED display manager for Heltec SSD1306
// - Optional: can be disabled at runtime
// - Supports a default homescreen renderer
// - Supports temporary per-task debug screens with timeout

#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "logo.h"

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
enum class HeaderRightMode : uint8_t { SignalBars = 0, PeerCount = 1, WifiStatus = 2 };

class OledDisplay {
 public:
  OledDisplay()
      : display(OLED_I2C_ADDR, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED), initialized(false) {
  }

  explicit OledDisplay(uint8_t i2cAddress)
      : display(i2cAddress, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED), initialized(false) {
  }

  // Safe begin that prevents double initialization
  // Returns true if initialization was performed, false if already initialized
  bool safeBegin(bool enable) {
    if (initialized) {
      return false; // Already initialized
    }
    unsafeBegin(enable);
    return true;
  }

private:
  // Internal unsafe begin - should not be called directly
  void unsafeBegin(bool enable) {
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

    initialized = true;
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
    const uint8_t clamped = percent > 100 ? 100 : percent;
    if (!batteryFilterInitialized) {
      batteryPercentFiltered = (float)clamped;
      batteryFilterInitialized = true;
    } else {
      // Low-pass filter to stabilize UI bars and reduce flicker
      const float alpha = 0.30f; // weight of new sample
      batteryPercentFiltered = (1.0f - alpha) * batteryPercentFiltered + alpha * (float)clamped;
    }
    batteryPercent = clamped;
  }

  // Optional charging indicator overlay
  void setBatteryCharging(bool charging) { batteryCharging = charging; }

  // Header right area configuration
  void setHeaderRightMode(HeaderRightMode mode) { headerRightMode = mode; }
  void setPeerCount(uint16_t count) { headerPeerCount = count; }
  void setWifiStatus(bool connected, int8_t signalStrength = -1) {
    wifiConnected = connected;
    wifiSignalStrength = signalStrength;
  }
  // Optional: show a tiny WiFi icon on the left header area (near battery)
  void setShowWifiMiniIconInHeaderLeft(bool show) { showWifiMiniIcon = show; }

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
      const uint8_t percentToDraw = (uint8_t)(batteryPercentFiltered + 0.5f);
      drawBatteryIcon(display, battX, battY, 16, 8, showBars ? percentToDraw : 255 /*outline-only*/);
      if (batteryCharging) {
        drawChargingBolt(display, battX, battY, 16, 8);
      }

      // Optional tiny WiFi icon just to the right of battery
      if (showWifiMiniIcon) {
        const int16_t wifiX = battX + 16 + 4; // battery width + margin
        const int16_t wifiY = 1;
        drawWifiMiniIcon(display, wifiX, wifiY);
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

  void drawWifiStatus(SSD1306Wire &d) {
    const int16_t topY = 0;
    const int16_t headerH = 10;
    const int16_t iconW = 14; // fixed width for WiFi fan icon
    const int16_t startX = d.width() - iconW;
    const int16_t cx = startX + iconW / 2;
    const int16_t cy = topY + headerH - 1; // baseline near bottom of header

    // Helper: plot upper half of a circle (simple arc) with integer math
    auto plotUpperArc = [&](int r) {
      for (int x = -r; x <= r; x++) {
        int rr = r * r;
        int xx = x * x;
        int y = 0;
        if (xx <= rr) {
          // y = floor(sqrt(r^2 - x^2))
          y = (int)sqrtf((float)(rr - xx));
        }
        // Thicken the arc slightly for visibility
        d.setPixel(cx + x, cy - y);
        if (y > 0) d.setPixel(cx + x, cy - y - 1);
      }
    };

    // Draw base icon when disconnected: faint arcs + slash
    if (!wifiConnected) {
      plotUpperArc(6);
      plotUpperArc(4);
      plotUpperArc(2);
      // Small dot at the base
      d.setPixel(cx, cy);
      d.setPixel(cx + 1, cy);
      d.setPixel(cx, cy - 1);
      d.setPixel(cx + 1, cy - 1);
      // Diagonal slash to indicate disconnected
      d.drawLine(startX, topY + 1, startX + iconW - 1, topY + headerH - 2);
      return;
    }

    // Map 0-100% to 1..3 arcs (plus base dot)
    uint8_t level = 0;
    if (wifiSignalStrength >= 0) {
      if (wifiSignalStrength <= 33) level = 1;
      else if (wifiSignalStrength <= 66) level = 2;
      else level = 3;
    }

    // Base dot (2x2 pixel block)
    d.setPixel(cx, cy);
    d.setPixel(cx + 1, cy);
    d.setPixel(cx, cy - 1);
    d.setPixel(cx + 1, cy - 1);

    if (level >= 1) plotUpperArc(2);
    if (level >= 2) plotUpperArc(4);
    if (level >= 3) plotUpperArc(6);
  }

  void drawWifiMiniIcon(SSD1306Wire &d, int16_t x, int16_t y) {
    // 10x8 mini fan-style icon with arcs and dot; draw diagonal slash if disconnected
    const int16_t iconW = 10;
    const int16_t cx = x + iconW / 2;
    const int16_t cy = y + 7; // baseline at bottom of 8px box

    auto plotUpperArc = [&](int r) {
      for (int dx = -r; dx <= r; dx++) {
        int rr = r * r;
        int xx = dx * dx;
        int yy = 0;
        if (xx <= rr) yy = (int)sqrtf((float)(rr - xx));
        d.setPixel(cx + dx, cy - yy);
      }
    };

    // Base dot (1x2)
    d.setPixel(cx, cy);
    d.setPixel(cx, cy - 1);

    if (!wifiConnected) {
      plotUpperArc(3);
      // Slash
      d.drawLine(x, y, x + iconW - 1, y + 7);
      return;
    }

    uint8_t level = 0;
    if (wifiSignalStrength >= 0) {
      if (wifiSignalStrength <= 33) level = 1;
      else if (wifiSignalStrength <= 66) level = 2;
      else level = 3;
    }

    if (level >= 1) plotUpperArc(2);
    if (level >= 2) plotUpperArc(3);
    if (level >= 3) plotUpperArc(4);
  }

  void drawHeaderRight(SSD1306Wire &d) {
    if (headerRightMode == HeaderRightMode::SignalBars) {
      drawLoraSignal(d);
    } else if (headerRightMode == HeaderRightMode::PeerCount) {
      drawPeersCount(d);
    } else if (headerRightMode == HeaderRightMode::WifiStatus) {
      drawWifiStatus(d);
    }
  }

  // Draw a modern, animated battery icon with smooth fill and charging indicators
  void drawBatteryIcon(SSD1306Wire &d, int16_t x, int16_t y, int16_t bodyW, int16_t bodyH, uint8_t percent) {
    // Ensure minimum dimensions for visual clarity
    if (bodyW < 14) bodyW = 14;
    if (bodyH < 8) bodyH = 8;

    // Draw battery outline with rounded corners
    d.drawRect(x, y, bodyW, bodyH);
    // Tip with improved proportions
    const int16_t tipW = 2;
    const int16_t tipH = max<int16_t>(4, bodyH / 2);
    const int16_t tipY = y + ((bodyH - tipH) / 2);
    d.fillRect(x + bodyW, tipY, tipW, tipH);

    // Inner area with margin for fill
    const int16_t ix = x + 2; // Increased margin for better look
    const int16_t iy = y + 2;
    const int16_t iw = bodyW - 4;
    const int16_t ih = bodyH - 4;

    if (percent <= 100) {
      // Calculate fill width based on percentage
      const int16_t fillW = (int16_t)((iw * percent) / 100);
      
      // Draw fill pattern based on percentage
      if (percent <= 15) {
        // Low battery: Draw striped pattern
        for (int16_t fx = ix; fx < ix + fillW; fx += 2) {
          d.fillRect(fx, iy, 1, ih);
        }
      } else {
        // Normal levels: Solid fill with subtle gradient effect
        d.fillRect(ix, iy, fillW, ih);
        
        // Add highlight line at top for 3D effect
        if (fillW > 2) {
          d.setPixel(ix + 1, iy);
          d.setPixel(ix + fillW - 2, iy);
        }
      }
    }
  }

  // Draw an animated charging indicator
  void drawChargingBolt(SSD1306Wire &d, int16_t x, int16_t y, int16_t bodyW, int16_t bodyH) {
    static uint32_t lastAnimMs = 0;
    static uint8_t animPhase = 0;
    const uint32_t nowMs = millis();
    
    // Update animation phase every 250ms
    if (nowMs - lastAnimMs >= 250) {
      animPhase = (animPhase + 1) % 4;
      lastAnimMs = nowMs;
    }

    // Center the charging indicator
    const int16_t ix = x + 2;
    const int16_t iy = y + 1;
    const int16_t iw = bodyW - 4;
    const int16_t ih = bodyH - 2;

    // Draw animated charging arrows
    const int16_t arrowH = ih / 2;
    const int16_t arrowW = min<int16_t>(6, iw / 2);
    const int16_t centerX = ix + iw / 2;
    
    // Draw two arrows that alternate visibility
    if (animPhase < 2) {
      // Upper arrow
      const int16_t y1 = iy + 1;
      d.drawLine(centerX - 2, y1 + arrowH - 1, centerX, y1);
      d.drawLine(centerX, y1, centerX + 2, y1 + arrowH - 1);
      d.drawLine(centerX, y1, centerX, y1 + arrowH);
    }
    if (animPhase > 1) {
      // Lower arrow
      const int16_t y2 = iy + ih - arrowH;
      d.drawLine(centerX - 2, y2 + arrowH - 1, centerX, y2);
      d.drawLine(centerX, y2, centerX + 2, y2 + arrowH - 1);
      d.drawLine(centerX, y2, centerX, y2 + arrowH);
    }
  }

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
  bool initialized;
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
  bool batteryFilterInitialized = false;
  float batteryPercentFiltered = 0.0f;

  // Header configuration
  HeaderRightMode headerRightMode = HeaderRightMode::SignalBars;
  uint16_t headerPeerCount = 0;
  bool wifiConnected = false;
  int8_t wifiSignalStrength = -1;
  bool showWifiMiniIcon = false;

  SSD1306Wire display;
};


