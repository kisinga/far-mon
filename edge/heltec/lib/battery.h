#pragma once

#include <Arduino.h>
#include "HT_SSD1306Wire.h"

namespace Battery {

struct Config {
  // Set adcPin to 0xFF to disable reading (icon will still render outline)
  uint8_t adcPin = 0xFF;
  // ADC reference voltage (Volts) for mapping raw -> volts
  float adcReferenceVoltage = 3.30f;
  // Input voltage divider ratio: Vbattery = Vadc * dividerRatio
  // Set to 1.0 if battery is directly sensed (not recommended)
  float dividerRatio = 2.00f; // e.g., 100k:100k divider
  // Battery voltage curve (Volts)
  float voltageEmpty = 3.30f;
  float voltageFull = 4.20f;
};

// Returns true and writes outPercent [0..100] when reading is available.
// Returns false if adcPin is disabled or any error; caller may render outline-only.
inline bool readPercent(const Config &cfg, uint8_t &outPercent) {
  if (cfg.adcPin == 0xFF) return false;
  int raw = analogRead(cfg.adcPin); // 0..4095
  if (raw < 0) return false;
  float vAdc = (float)raw * (cfg.adcReferenceVoltage / 4095.0f);
  float vBat = vAdc * cfg.dividerRatio;
  float pct = (vBat - cfg.voltageEmpty) * (100.0f / (cfg.voltageFull - cfg.voltageEmpty));
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  outPercent = (uint8_t)(pct + 0.5f);
  return true;
}

// Draw a compact battery icon. If percent is >100 (e.g., 255), draws outline only.
inline void drawIcon(SSD1306Wire &d, int16_t x, int16_t y, int16_t bodyW, int16_t bodyH, uint8_t percent) {
  if (bodyW < 10) bodyW = 10;
  if (bodyH < 6) bodyH = 6;
  const int16_t tipW = 2;
  // Body outline
  d.drawRect(x, y, bodyW, bodyH);
  // Tip
  d.fillRect(x + bodyW, y + (bodyH / 4), tipW, bodyH - (bodyH / 2));
  // Fill
  if (percent <= 100) {
    int16_t fillW = (int16_t)((bodyW - 2) * percent / 100);
    if (fillW > 0) {
      d.fillRect(x + 1, y + 1, fillW, bodyH - 2);
    }
  }
}

} // namespace Battery


