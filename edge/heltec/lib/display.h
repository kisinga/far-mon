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

    // Minimal splash for bring-up
    display.clear();
    // Center the 63x63 logo on a 128x64 screen
    const int16_t logoX = (display.width() - logo_width) / 2;
    const int16_t logoY = (display.height() - logo_height) / 2;
    display.drawXbm(logoX, logoY, logo_width, logo_height, logo_bits);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(display.width() / 2, 0, F("Farm"));
    display.display();
    delay(1200);
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

    // Header line: device id and uptime (seconds)
    if (deviceId != nullptr) {
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.drawString(0, 0, String("ID:") + deviceId);
    }
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(display.width(), 0, String(nowMs / 1000) + String("s"));

    // Content area
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    if (debugCb != nullptr && timeNotExpired(nowMs, debugUntilMs)) {
      // Draw debug content starting at y=12
      display.drawHorizontalLine(0, 10, display.width());
      display.setColor(WHITE);
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      // Push origin by drawing offset; renderers should draw within 0..(w,h)
      debugCb(display, debugCtx);
    } else if (homescreenCb != nullptr) {
      display.drawHorizontalLine(0, 10, display.width());
      homescreenCb(display, homescreenCtx);
      // Clear debug state once expired
      debugCb = nullptr;
      debugCtx = nullptr;
      debugUntilMs = 0;
    } else {
      // Default empty screen
      display.drawHorizontalLine(0, 10, display.width());
      display.drawString(0, 14, F("No homescreen set"));
    }

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

  bool enabled = false;
  const char *deviceId = nullptr;

  int8_t vextPinOverride = -1; // -1 means use Vext macro if available

  RenderCallback homescreenCb = nullptr;
  void *homescreenCtx = nullptr;

  RenderCallback debugCb = nullptr;
  void *debugCtx = nullptr;
  uint32_t debugUntilMs = 0;

  SSD1306Wire display;
};


