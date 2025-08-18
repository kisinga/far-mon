// Header-only debug router to consolidate debug info to Serial and OLED overlay
// Distinction preserved: debug overlays/logs are separate from the default homescreen

#pragma once

#include <Arduino.h>
#include "display.h"

using SerialRenderCallback = void (*)(Print &out, void *context);

class DebugRouter {
 public:
  DebugRouter() : serialEnabled(true), display(nullptr), deviceId(nullptr) {}

  void begin(bool enableSerial, OledDisplay *oledDisplay, const char *devId) {
    serialEnabled = enableSerial;
    display = oledDisplay;
    deviceId = devId;
  }

  void setSerialEnabled(bool enabled) { serialEnabled = enabled; }
  void setDisplay(OledDisplay *oledDisplay) { display = oledDisplay; }

  void debug(RenderCallback oledCb,
             void *oledCtx,
             SerialRenderCallback serialCb,
             void *serialCtx,
             uint32_t nowMs,
             uint32_t durationMs) {
    if (display != nullptr) {
      display->showDebug(oledCb, oledCtx, nowMs, durationMs);
    }
    if (serialEnabled) {
      Serial.print(F("[debug] t="));
      Serial.print(nowMs);
      if (deviceId != nullptr) {
        Serial.print(F(" id="));
        Serial.print(deviceId);
      }
      Serial.print(F(" | "));
      if (serialCb != nullptr) {
        serialCb(Serial, serialCtx);
      }
      Serial.println();
    }
  }

  // Convenience: duration from current millis()
  void debugFor(RenderCallback oledCb,
                void *oledCtx,
                SerialRenderCallback serialCb,
                void *serialCtx,
                uint32_t durationMs) {
    debug(oledCb, oledCtx, serialCb, serialCtx, millis(), durationMs);
  }

 private:
  bool serialEnabled;
  OledDisplay *display;
  const char *deviceId;
};


