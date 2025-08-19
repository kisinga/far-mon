#pragma once

#include <Arduino.h>
#include "HT_SSD1306Wire.h"

namespace Battery {

// Optional control pin for VBAT measurement gating. If either of these is
// defined, we'll pull the control pin LOW for a short time before sampling and
// then return it to INPUT to minimize power/leakage.
#if defined(BATTERY_CTRL_PIN)
#define BATTERY__CTRL_PIN BATTERY_CTRL_PIN
#elif defined(VBAT_CTRL)
#define BATTERY__CTRL_PIN VBAT_CTRL
#endif

struct Config {
  // Set adcPin to 0xFF to disable reading (icon will still render outline)
  uint8_t adcPin = 0xFF;
  // Use calibrated mV read when available (ESP32-S2/S3/ESP32)
  bool useCalibratedMv = true;
  // Optional: set ADC attenuation once on first read (11dB recommended for VBAT via divider)
  bool setAttenuationOnFirstRead = false;
  // Input voltage divider ratio: Vbattery = Vadc * dividerRatio
  // Example: 100k:100k -> 2.0f; set to 1.0 if battery is directly sensed (not recommended)
  float dividerRatio = 2.00f;
  // Battery voltage curve bounds (for clamping only)
  float voltageEmpty = 3.30f;
  float voltageFull = 4.20f;
  // Number of ADC samples for smoothing (min 1). We will drop min/max when n>=4
  uint8_t samples = 8;
  // Optional: control pin that enables VBAT sense path (active LOW). -1 to disable.
  int8_t ctrlPin = -1;
  // Use Heltec V3 empirical scaling (raw/238.7) instead of calibrated mV + divider
  bool useHeltecV3Scaling = true;
  // Internal: one-time attenuation applied
  bool _attenuationApplied = false;
};

// Returns true and writes outPercent [0..100] when reading is available.
// Returns false if adcPin is disabled or any error; caller may render outline-only.
inline uint16_t readBatteryMilliVolts(Config &cfg, bool &ok) {
  ok = false;
  if (cfg.adcPin == 0xFF) return 0;
  
  // Optionally enable VBAT sense path via control pin
  if (cfg.ctrlPin >= 0) {
    pinMode((uint8_t)cfg.ctrlPin, OUTPUT);
    digitalWrite((uint8_t)cfg.ctrlPin, LOW);
    delay(5);
  }

  // Collect samples (basic smoothing; drop min/max when we have enough)
  const uint8_t n = cfg.samples < 1 ? 1 : cfg.samples;
  uint32_t sum = 0;
  uint16_t vmin = 65535, vmax = 0;
  for (uint8_t i = 0; i < n; i++) {
    uint16_t sample = 0;
    if (cfg.useHeltecV3Scaling) {
      // Read raw and defer scaling to the end (raw/238.7 -> Volts)
      int raw = analogRead(cfg.adcPin);
      if (raw < 0) raw = 0;
      sample = (uint16_t)raw;
    } else if (cfg.useCalibratedMv) {
      sample = (uint16_t)analogReadMilliVolts(cfg.adcPin);
    } else {
      int raw = analogRead(cfg.adcPin);
      if (raw < 0) raw = 0;
      sample = (uint16_t)((raw * 1100UL) / 4095UL);
    }
    sum += sample;
    if (sample < vmin) vmin = sample;
    if (sample > vmax) vmax = sample;
    delayMicroseconds(200);
  }

  uint32_t adjSum = sum;
  uint8_t adjN = n;
  if (n >= 4) {
    adjSum = sum - vmin - vmax;
    adjN = n - 2;
  }
  if (adjN == 0) adjN = 1;
  uint32_t vBatMv = 0;

  if (cfg.useHeltecV3Scaling) {
    // Average raw and apply empirical scaling constant to get Volts
    const float rawAvg = (float)(adjSum / adjN);
    const float vBat = rawAvg / 238.7f; // Volts
    vBatMv = (uint32_t)(vBat * 1000.0f + 0.5f);
  } else {
    // One-time attenuation (best-effort; API varies across cores)
    if (cfg.setAttenuationOnFirstRead && !cfg._attenuationApplied) {
      #if defined(ESP32)
      ::analogSetPinAttenuation(cfg.adcPin, ADC_11db);
      #endif
      cfg._attenuationApplied = true;
    }
    const uint16_t vAdcMv = (uint16_t)(adjSum / adjN);
    vBatMv = (uint32_t)((float)vAdcMv * cfg.dividerRatio + 0.5f);
  }

  // Return control pin to input to save power/leakage
  if (cfg.ctrlPin >= 0) {
    pinMode((uint8_t)cfg.ctrlPin, INPUT);
  }

  ok = true;
  return (uint16_t)(vBatMv > 65535 ? 65535 : vBatMv);
}

inline uint8_t mapVoltageToPercent(float vBat) {
  // Tuned curve based on measured discharge profile (ported from heltec_unofficial)
  static const float min_voltage = 3.04f;
  static const float max_voltage = 4.26f;
  static const uint8_t scaled_voltage[100] = {
    254, 242, 230, 227, 223, 219, 215, 213, 210, 207,
    206, 202, 202, 200, 200, 199, 198, 198, 196, 196,
    195, 195, 194, 192, 191, 188, 187, 185, 185, 185,
    183, 182, 180, 179, 178, 175, 175, 174, 172, 171,
    170, 169, 168, 166, 166, 165, 165, 164, 161, 161,
    159, 158, 158, 157, 156, 155, 151, 148, 147, 145,
    143, 142, 140, 140, 136, 132, 130, 130, 129, 126,
    125, 124, 121, 120, 118, 116, 115, 114, 112, 112,
    110, 110, 108, 106, 106, 104, 102, 101, 99, 97,
    94, 90, 81, 80, 76, 73, 66, 52, 32, 7,
  };
  // Compute threshold table step size
  const float step = (max_voltage - min_voltage) / 256.0f;
  for (int n = 0; n < 100; n++) {
    const float threshold = min_voltage + (step * (float)scaled_voltage[n]);
    if (vBat > threshold) {
      int p = 100 - n;
      if (p < 0) p = 0; if (p > 100) p = 100;
      return (uint8_t)p;
    }
  }
  return 0;
}

inline bool readPercent(Config &cfg, uint8_t &outPercent) {
  bool ok = false;
  uint16_t vBatMv = readBatteryMilliVolts(cfg, ok);
  if (!ok) return false;
  float vBat = vBatMv / 1000.0f;
  // Clamp to configured bounds before mapping
  if (vBat < cfg.voltageEmpty) vBat = cfg.voltageEmpty;
  if (vBat > cfg.voltageFull) vBat = cfg.voltageFull;
  outPercent = mapVoltageToPercent(vBat);
  return true;
}

// Draw a compact, pixel-accurate battery icon with 4 bars.
// If percent > 100 (e.g., 255), only the outline is drawn (no bars).
inline void drawIcon(SSD1306Wire &d, int16_t x, int16_t y, int16_t bodyW, int16_t bodyH, uint8_t percent) {
  // Clamp minimums to ensure shape integrity
  if (bodyW < 12) bodyW = 12;
  if (bodyH < 8) bodyH = 8;

  // Body outline
  d.drawRect(x, y, bodyW, bodyH);
  // Tip: fixed 2px width, vertically centered to body height
  const int16_t tipW = 2;
  const int16_t tipH = max<int16_t>(3, bodyH / 2);
  const int16_t tipY = y + ((bodyH - tipH) / 2);
  d.fillRect(x + bodyW, tipY, tipW, tipH);

  // Inner drawable area (1px margin)
  const int16_t ix = x + 1;
  const int16_t iy = y + 1;
  const int16_t iw = bodyW - 2;
  const int16_t ih = bodyH - 2;

  // Bar layout: 4 bars, fixed 1px gaps, bars as wide as will fit (>=1px)
  const int8_t bars = 4;
  const int16_t gap = 1; // 1px between bars; borders handled via centering
  int16_t barW = (iw - (bars - 1) * gap) / bars;
  if (barW < 1) barW = 1;
  // Keep bars visually thicker than or equal to gap when possible
  if (barW < 2 && iw >= (bars * 2 + (bars - 1) * gap)) barW = 2;

  const int16_t usedW = (bars * barW) + ((bars - 1) * gap);
  const int16_t startX = ix + max<int16_t>(0, (iw - usedW) / 2);

  // Map percent to number of filled bars (exact quartiles)
  uint8_t barsFilled = 0;
  if (percent <= 100) {
    barsFilled = (percent == 0) ? 0 : (uint8_t)(1 + ((percent - 1) / 25));
  }

  for (int i = 0; i < bars; i++) {
    const int16_t bx = startX + i * (barW + gap);
    if (percent <= 100 && i < barsFilled) {
      d.fillRect(bx, iy, barW, ih);
    } else {
      // Draw empty bar outlines very faintly only when space allows
      if (barW >= 2 && ih >= 4) {
        d.drawRect(bx, iy, barW, ih);
      }
    }
  }
}

// Draw a small lightning bolt centered inside the battery body interior.
inline void drawChargingBolt(SSD1306Wire &d, int16_t x, int16_t y, int16_t bodyW, int16_t bodyH) {
  // Compute inner area
  const int16_t ix = x + 1;
  const int16_t iy = y + 1;
  const int16_t iw = bodyW - 2;
  const int16_t ih = bodyH - 2;
  // Bolt bounds (7x6) centered
  const int16_t bw = min<int16_t>(7, max<int16_t>(5, iw - 2));
  const int16_t bh = min<int16_t>(6, max<int16_t>(4, ih - 2));
  const int16_t bx = ix + (iw - bw) / 2;
  const int16_t by = iy + (ih - bh) / 2;

  // Draw a zig-zag bolt using lines; coordinates relative to (bx,by)
  // Shape:
  //  (0,1)->(2,1)->(1,3)->(4,0)->(3,2)->(5,2)
  const int16_t x0 = bx + 0, y0 = by + (bh >= 6 ? 1 : 0);
  const int16_t x1 = bx + (bw >= 6 ? 2 : 1), y1 = y0;
  const int16_t x2 = bx + (bw >= 6 ? 1 : 1), y2 = by + (bh >= 6 ? 3 : max<int16_t>(2, bh - 2));
  const int16_t x3 = bx + (bw >= 7 ? 4 : min<int16_t>(3, bw - 1)), y3 = by + 0;
  const int16_t x4 = bx + (bw >= 7 ? 3 : min<int16_t>(2, bw - 1)), y4 = by + (bh >= 6 ? 2 : 1);
  const int16_t x5 = bx + (bw >= 7 ? 5 : min<int16_t>(4, bw - 1)), y5 = y4;

  d.drawLine(x0, y0, x1, y1);
  d.drawLine(x1, y1, x2, y2);
  d.drawLine(x2, y2, x3, y3);
  d.drawLine(x3, y3, x4, y4);
  d.drawLine(x4, y4, x5, y5);
}

} // namespace Battery


