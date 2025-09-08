// Header-only simple logger with verbosity, OLED overlay, and debug routing capabilities
// - Centralizes Serial and OLED logging
// - Supports temporary debug overlays with duration
// - Default: verbose=false; level=Info; Serial on

#pragma once

#include <Arduino.h>
#include "display.h"

using DebugRenderCallback = void (*)(SSD1306Wire &display, void *context);
using DebugSerialCallback = void (*)(Print &out, void *context);

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

// Internal initialization function - not exposed publicly to prevent unsafe usage
namespace internal {
inline void initializeUnsafe(OledDisplay *display, const char *deviceId) {
  begin(true, display, deviceId);
  setLevel(Level::Info);
  setVerbose(false);
}
}

// Safe initialization that prevents double initialization
// This is the ONLY public initialization method - follows safe DI pattern
// Returns true if initialization was performed, false if already initialized
inline bool safeInitialize(OledDisplay *display, const char *deviceId) {
  // Check if already initialized by verifying display is set
  if (g_display != nullptr) {
    return false; // Already initialized
  }
  internal::initializeUnsafe(display, deviceId);
  return true;
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

// Optional: unprefixed raw line output that still respects level and serial enable
inline void rawf(Level level, const char *fmt, ...) {
  if (!isEnabled(level) || !g_serialEnabled) return;
  char buf[160];
  va_list ap; va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.println(buf);
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

// Debug routing functionality (consolidated from DebugRouter)
// Shows temporary debug overlay with optional serial output
inline void debug(DebugRenderCallback oledCb,
                  void *oledCtx,
                  DebugSerialCallback serialCb,
                  void *serialCtx,
                  uint32_t nowMs,
                  uint32_t durationMs) {
  // Handle OLED display
  if (g_display != nullptr && oledCb != nullptr) {
    g_display->showDebug(oledCb, oledCtx, nowMs, durationMs);
  }

  // Handle serial output
  if (g_serialEnabled && serialCb != nullptr) {
    Serial.print(F("[debug] t="));
    Serial.print(nowMs);
    if (g_deviceId != nullptr) {
      Serial.print(F(" id="));
      Serial.print(g_deviceId);
    }
    Serial.print(F(" | "));
    serialCb(Serial, serialCtx);
    Serial.println();
  }
}

// Convenience method: duration from current millis()
inline void debugFor(DebugRenderCallback oledCb,
                     void *oledCtx,
                     DebugSerialCallback serialCb,
                     void *serialCtx,
                     uint32_t durationMs) {
  debug(oledCb, oledCtx, serialCb, serialCtx, millis(), durationMs);
}

} // namespace Logger

// Convenience logging macros for concise call sites
#ifndef LOGE
#define LOGE(tag, fmt, ...) Logger::printf(Logger::Level::Error,   tag, fmt, ##__VA_ARGS__)
#endif
#ifndef LOGW
#define LOGW(tag, fmt, ...) Logger::printf(Logger::Level::Warn,    tag, fmt, ##__VA_ARGS__)
#endif
#ifndef LOGI
#define LOGI(tag, fmt, ...) Logger::printf(Logger::Level::Info,    tag, fmt, ##__VA_ARGS__)
#endif
#ifndef LOGD
#define LOGD(tag, fmt, ...) Logger::printf(Logger::Level::Debug,   tag, fmt, ##__VA_ARGS__)
#endif
#ifndef LOGV
#define LOGV(tag, fmt, ...) Logger::printf(Logger::Level::Verbose, tag, fmt, ##__VA_ARGS__)
#endif

// Anti-spam helpers
#ifndef LOG_CONCAT_INNER
#define LOG_CONCAT_INNER(a,b) a##b
#endif
#ifndef LOG_CONCAT
#define LOG_CONCAT(a,b) LOG_CONCAT_INNER(a,b)
#endif
#ifndef LOG_UNIQUE_NAME
#define LOG_UNIQUE_NAME(base) LOG_CONCAT(base, __LINE__)
#endif

#ifndef LOG_EVERY_MS
#define LOG_EVERY_MS(intervalMs, code_block) \
  do { static uint32_t LOG_UNIQUE_NAME(_last__) = 0; uint32_t _now__ = millis(); \
       if (_now__ - LOG_UNIQUE_NAME(_last__) >= (uint32_t)(intervalMs)) { LOG_UNIQUE_NAME(_last__) = _now__; code_block; } } while(0)
#endif

#ifndef LOG_ON_CHANGE
#define LOG_ON_CHANGE(expr, code_block) \
  do { static auto LOG_UNIQUE_NAME(_prev__) = (expr); auto _cur__ = (expr); \
       if (_cur__ != LOG_UNIQUE_NAME(_prev__)) { LOG_UNIQUE_NAME(_prev__) = _cur__; code_block; } } while(0)
#endif


