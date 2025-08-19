// Header-only simple logger with verbosity and optional OLED overlay
// - Centralizes Serial and OLED logging
// - Default: verbose=false; level=Info; Serial on

#pragma once

#include <Arduino.h>
#include "display.h"

namespace Logger {

enum class Level : uint8_t { Error = 0, Warn = 1, Info = 2, Debug = 3, Verbose = 4 };

struct OverlayCtx {
  char line1[22];
  char line2[22];
};

inline bool g_serialEnabled = true;
inline Level g_level = Level::Info;
inline bool g_verbose = false;
inline OledDisplay *g_display = nullptr;
inline const char *g_deviceId = nullptr;
inline OverlayCtx g_overlayCtx; // reused buffer

inline void begin(bool enableSerial, OledDisplay *display, const char *deviceId) {
  g_serialEnabled = enableSerial;
  g_display = display;
  g_deviceId = deviceId;
}

inline void setLevel(Level level) { g_level = level; }
inline void setVerbose(bool verbose) { g_verbose = verbose; }
inline void attachDisplay(OledDisplay *display, const char *deviceId) {
  g_display = display;
  g_deviceId = deviceId;
}

inline bool isEnabled(Level level) {
  if (g_verbose) return true;
  return static_cast<uint8_t>(level) <= static_cast<uint8_t>(g_level);
}

inline void vprintf(Level level, const char *tag, const char *fmt, va_list ap) {
  if (!isEnabled(level)) return;
  if (g_serialEnabled) {
    char buf[160];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    Serial.print('[');
    if (tag) Serial.print(tag); else Serial.print(F("log"));
    Serial.print(']');
    if (g_deviceId) {
      Serial.print(' ');
      Serial.print(g_deviceId);
    }
    Serial.print(' ');
    Serial.println(buf);
  }
}

inline void printf(Level level, const char *tag, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  vprintf(level, tag, fmt, ap);
  va_end(ap);
}

inline void overlay(const char *line1, const char *line2, uint32_t nowMs, uint32_t durationMs) {
  if (g_display == nullptr) return;
  strncpy(g_overlayCtx.line1, line1 ? line1 : "", sizeof(g_overlayCtx.line1) - 1);
  g_overlayCtx.line1[sizeof(g_overlayCtx.line1) - 1] = '\0';
  strncpy(g_overlayCtx.line2, line2 ? line2 : "", sizeof(g_overlayCtx.line2) - 1);
  g_overlayCtx.line2[sizeof(g_overlayCtx.line2) - 1] = '\0';
  g_display->showDebug(
    [](SSD1306Wire &d, void *ctx){
      OverlayCtx *oc = static_cast<OverlayCtx*>(ctx);
      d.setTextAlignment(TEXT_ALIGN_LEFT);
      d.drawString(0, 14, oc->line1);
      d.drawString(0, 28, oc->line2);
    },
    &g_overlayCtx,
    nowMs,
    durationMs
  );
}

} // namespace Logger


