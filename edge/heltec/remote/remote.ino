// Minimal, extendable coordination logic for Heltec/ESP32 remote node
// - Uses shared scheduler from lib/scheduler.h
// - Device-specific AppState remains local to this file
// - Example tasks: heartbeat, analog input reader, periodic reporter

#include <Arduino.h>
#include "../lib/scheduler.h"
#include "../lib/display.h"
#include "../lib/debug.h"
#include "../lib/logger.h"
#include "../lib/board_config.h"
// Align LoRa RF params with master
#ifndef LORA_COMM_RF_FREQUENCY
#define LORA_COMM_RF_FREQUENCY 868000000UL
#endif
#ifndef LORA_COMM_TX_POWER_DBM
#define LORA_COMM_TX_POWER_DBM 14
#endif
#include "../lib/lora_comm.h"

// ===== Configuration =====
// Change to any valid ADC-capable pin on your board. Heltec LoRa 32 commonly uses GPIO34.
static const uint8_t ANALOG_INPUT_PIN = 34; // ADC1 channel
static const float ANALOG_REFERENCE_VOLTAGE = 3.30f; // Volts
static const bool ENABLE_OLED_DISPLAY = true; // Set false to disable screen usage at runtime
static const uint8_t MASTER_NODE_ID = 1; // Destination for data frames

// Device identity (1-byte id encoded as two ASCII digits to match docs)
static const char DEVICE_ID[] = "03";

// Task cadence
static const uint32_t HEARTBEAT_INTERVAL_MS = 1000;   // 1s
static const uint32_t ANALOG_READ_INTERVAL_MS = 200;  // 5 Hz sampling
static const uint32_t REPORT_INTERVAL_MS = 2000;      // 2s

// ===== Device-specific Application State =====
struct AppState {
  // Time
  uint32_t nowMs = 0;

  // Analog sensor
  int analogRaw = 0;         // 0..4095 on ESP32
  float analogVoltage = 0.0; // Volts

  // Heartbeat
  bool heartbeatOn = false;
  // Reporting jitter scheduler
  uint32_t nextReportDueMs = 0;
};

static AppState appState;
static TaskScheduler<AppState, 8> scheduler;
static OledDisplay oled;
static DebugRouter debugRouter;
static LoRaComm lora;
static volatile uint8_t g_lastAckSrc = 0;

// ===== Optional Battery Reading (modularized) =====
#include "../lib/battery.h"
static Battery::Config g_battCfg;
// Optional charge status pin and polarity from board config
#ifdef CHARGE_STATUS_PIN
static const int8_t kChargeStatusPin = CHARGE_STATUS_PIN;
#else
static const int8_t kChargeStatusPin = -1;
#endif
#ifdef CHARGE_STATUS_ACTIVE_LOW
static const bool kChargeActiveLow = (CHARGE_STATUS_ACTIVE_LOW != 0);
#else
static const bool kChargeActiveLow = true;
#endif

// Debounced charge detection state (assume active-low STAT: 0 = charging)
struct ChargeDetectState {
  int8_t pin;
  bool activeLow;
  bool isChargingStable;
  uint8_t lowStreak;
  uint8_t highStreak;
  uint32_t lastChangeMs;
};
static ChargeDetectState g_chargeState = { kChargeStatusPin, kChargeActiveLow, false, 0, 0, 0 };
// Track GPIO reliability and a voltage-slope fallback detector
static bool g_chargeGpioSawLow = false;
static bool g_chargeGpioSawHigh = false;
static bool g_chargeGpioReliable = false;
static uint16_t g_lastVBatMv = 0;
static uint32_t g_lastVBatMs = 0;
static bool g_fallbackCharging = false;

// ===== LoRa callbacks =====
static void onLoraData(uint8_t src, const uint8_t *payload, uint8_t len) {
  (void)src;
  (void)payload;
  (void)len;
  // Keep minimal; data ACK is auto-handled by lora_comm
}

static void onLoraAck(uint8_t src, uint16_t msgId) {
  (void)msgId;
  g_lastAckSrc = src;
  // Brief debug overlay to show connectivity
  debugRouter.debugFor(
    [](SSD1306Wire &d, void *ctx) {
      uint8_t s = ctx ? *static_cast<uint8_t*>(ctx) : 0;
      int16_t cx, cy, cw, ch;
      oled.getContentArea(cx, cy, cw, ch);
      d.setTextAlignment(TEXT_ALIGN_LEFT);
      d.drawString(cx, cy, F("ACK"));
      d.drawString(cx, cy + 14, String("from ") + String(s));
    },
    (void*)&g_lastAckSrc,
    [](Print &out, void *ctx) {
      uint8_t s = ctx ? *static_cast<uint8_t*>(ctx) : 0;
      out.print(F("ack from="));
      out.print(s);
    },
    (void*)&g_lastAckSrc,
    600
  );
}

// ===== Utilities =====
static float convertAdcToVolts(int adcRaw) {
  // ESP32 default ADC resolution is 12-bit (0..4095). Keep simple mapping here.
  const float scale = 1.0f / 4095.0f;
  return (float)adcRaw * scale * ANALOG_REFERENCE_VOLTAGE;
}

// ===== Tasks =====

static void taskHeartbeat(AppState &state) {
  state.heartbeatOn = !state.heartbeatOn;
  // Consolidated debug: serial + OLED overlay (800ms)
  debugRouter.debug(
    // OLED renderer
    [](SSD1306Wire &d, void *ctx) {
      (void)ctx;
      int16_t cx, cy, cw, ch;
      oled.getContentArea(cx, cy, cw, ch);
      d.setTextAlignment(TEXT_ALIGN_LEFT);
      d.drawString(cx, cy, F("Heartbeat"));
      d.drawString(cx, cy + 14, F("Toggled"));
    },
    nullptr,
    // Serial renderer
    [](Print &out, void *ctx) {
      AppState *s = static_cast<AppState*>(ctx);
      out.print(F("heartbeat on="));
      out.print(s->heartbeatOn ? F("1") : F("0"));
    },
    &appState,
    state.nowMs,
    800
  );
}

static void taskReadAnalog(AppState &state) {
  // Simple read; add averaging/median filtering later if needed
  const int raw = analogRead(ANALOG_INPUT_PIN);
  state.analogRaw = raw;
  state.analogVoltage = convertAdcToVolts(raw);

  // Consolidated debug: serial + OLED overlay (400ms)
  debugRouter.debug(
    // OLED renderer
    [](SSD1306Wire &d, void *ctx) {
      AppState *s = static_cast<AppState*>(ctx);
      int16_t cx, cy, cw, ch;
      oled.getContentArea(cx, cy, cw, ch);
      d.setTextAlignment(TEXT_ALIGN_LEFT);
      d.drawString(cx, cy, F("Analog"));
      d.drawString(cx, cy + 14, String("raw=") + String(s->analogRaw));
      d.drawString(cx, cy + 28, String("V=") + String(s->analogVoltage, 3));
    },
    &appState,
    // Serial renderer disabled to reduce verbosity; reports cover this
    nullptr,
    nullptr,
    state.nowMs,
    5
  );
}

static void taskReport(AppState &state) {
  // Respect jittered schedule to avoid herd effects
  if ((int32_t)(millis() - state.nextReportDueMs) < 0) {
    return;
  }

  // Minimal key=value CSV line as per docs (also sent over LoRa)
  Logger::rawf(Logger::Level::Info, "id=%s,ain_raw=%d,ain_v=%.3f", DEVICE_ID, state.analogRaw, state.analogVoltage);

  // Consolidated debug: serial + OLED overlay (700ms)
  debugRouter.debug(
    // OLED renderer
    [](SSD1306Wire &d, void *ctx) {
      (void)ctx;
      int16_t cx, cy, cw, ch;
      oled.getContentArea(cx, cy, cw, ch);
      d.setTextAlignment(TEXT_ALIGN_LEFT);
      d.drawString(cx, cy, F("Report"));
      d.drawString(cx, cy + 14, F("Sent"));
    },
    nullptr,
    // Serial renderer
    [](Print &out, void *ctx) {
      AppState *s = static_cast<AppState*>(ctx);
      out.print(F("report ain_raw="));
      out.print(s->analogRaw);
      out.print(F(" ain_v="));
      out.print(s->analogVoltage, 3);
    },
    &appState,
    state.nowMs,
    700
  );

  // LoRa: send compact telemetry to master; require ACK (also serves as liveness)
  char buf[48];
  int n = snprintf(buf, sizeof(buf), "id=%s,r=%d,v=%.3f", DEVICE_ID, state.analogRaw, state.analogVoltage);
  if (n > 0) {
    const bool queued = lora.sendData(MASTER_NODE_ID, (const uint8_t*)buf, (uint8_t)min(n, (int)sizeof(buf) - 1), /*requireAck=*/true);
    if (!queued) {
      LOGW("lora", "telemetry not queued (outbox full)");
    }
  }

  // Schedule next report with ±20% jitter
  const int32_t jitter = (int32_t)((int32_t)REPORT_INTERVAL_MS / 5); // 20%
  const int32_t delta = (int32_t)REPORT_INTERVAL_MS + (int32_t)random(-jitter, jitter + 1);
  // Clamp to minimum 100ms to avoid zero/negative intervals
  const int32_t safeDelta = (delta < 100) ? 100 : delta;
  state.nextReportDueMs = millis() + (uint32_t)safeDelta;
}

// Display homescreen renderer: called when no debug overlay is active
static void renderHome(SSD1306Wire &d, void *ctx) {
  AppState *s = static_cast<AppState*>(ctx);
  // Determine content area from layout manager
  int16_t cx, cy, cw, ch;
  oled.getContentArea(cx, cy, cw, ch);

  // Column layout within content area
  const int16_t leftX = cx;
  const int16_t rightX = cx + cw;

  // Left: Analog card
  d.setTextAlignment(TEXT_ALIGN_LEFT);
  d.setFont(ArialMT_Plain_10);
  d.drawString(leftX, cy, F("Analog"));
  d.setFont(ArialMT_Plain_16);
  d.drawString(leftX, cy + 12, String(s->analogVoltage, 3) + String(" V"));
  d.setFont(ArialMT_Plain_10);
  d.drawString(leftX, cy + 30, String("raw ") + String(s->analogRaw));

  // Right: Status card
  d.setTextAlignment(TEXT_ALIGN_RIGHT);
  d.setFont(ArialMT_Plain_10);
  d.drawString(rightX, cy, F("Status"));
  d.setFont(ArialMT_Plain_16);
  d.drawString(rightX, cy + 12, s->heartbeatOn ? F("HB ON") : F("HB OFF"));
  d.setFont(ArialMT_Plain_10);
  const bool linkOk = lora.isConnected();
  const int16_t rssi = lora.getLastRssiDbm();
  d.drawString(rightX, cy + 30, linkOk ? F("LNK OK") : F("LNK --"));
  if (linkOk) {
    d.drawString(rightX, cy + 40, String(rssi) + String(" dBm"));
  }
}

static void taskDisplay(AppState &state) {
  // Update header LoRa status
  oled.setLoraStatus(lora.isConnected(), lora.getLastRssiDbm());
  // Slave shows signal bars on right
  oled.setHeaderRightMode(HeaderRightMode::SignalBars);
  // Battery icon on left
  uint8_t bp = 255;
  bool haveBatt = Battery::readPercent(g_battCfg, bp);
  
  // Debug battery readings (anti-spam)
  bool ok = false;
  uint16_t vBatMv = Battery::readBatteryMilliVolts(g_battCfg, ok);
  if (ok) {
    LOG_EVERY_MS(5000, LOGI("batt", "voltage=%.2fV percent=%u", vBatMv / 1000.0f, (unsigned)bp); );
  } else {
    LOG_EVERY_MS(10000, LOGW("batt", "battery read failed"); );
  }
  // Update slope-based fallback (every >=5s)
  if (ok) {
    if (g_lastVBatMs == 0) {
      g_lastVBatMs = state.nowMs;
      g_lastVBatMv = vBatMv;
    } else {
      uint32_t dt = state.nowMs - g_lastVBatMs;
      if (dt >= 5000) {
        int32_t dv = (int32_t)vBatMv - (int32_t)g_lastVBatMv;
        if (dv >= 10) {
          g_fallbackCharging = true;
        } else if (dv <= -10) {
          g_fallbackCharging = false;
        }
        g_lastVBatMs = state.nowMs;
        g_lastVBatMv = vBatMv;
      }
    }
  }
  
  oled.setBatteryStatus(haveBatt, haveBatt ? bp : 0);
  if (kChargeStatusPin >= 0) {
    const int lv = digitalRead((uint8_t)kChargeStatusPin);
    const bool chargingSample = (g_chargeState.activeLow ? (lv == LOW) : (lv == HIGH));
    // Observe both levels to consider GPIO reliable
    if (lv == LOW) g_chargeGpioSawLow = true; else g_chargeGpioSawHigh = true;
    if (!g_chargeGpioReliable && g_chargeGpioSawLow && g_chargeGpioSawHigh) g_chargeGpioReliable = true;
    // Simple debounce: require 2 consecutive samples to change state
    if (chargingSample) {
      g_chargeState.lowStreak++;
      g_chargeState.highStreak = 0;
    } else {
      g_chargeState.highStreak++;
      g_chargeState.lowStreak = 0;
    }
    if (!g_chargeState.isChargingStable && g_chargeState.lowStreak >= 2) {
      g_chargeState.isChargingStable = true;
      g_chargeState.lastChangeMs = state.nowMs;
      LOGI("batt", "charging state -> yes");
    } else if (g_chargeState.isChargingStable && g_chargeState.highStreak >= 2) {
      g_chargeState.isChargingStable = false;
      g_chargeState.lastChangeMs = state.nowMs;
      LOGI("batt", "charging state -> no");
    }
    const bool useGpio = g_chargeGpioReliable;
    const bool chargingFinal = useGpio ? g_chargeState.isChargingStable : g_fallbackCharging;
    oled.setBatteryCharging(chargingFinal);
    LOG_EVERY_MS(5000, LOGD("batt", "charge_pin=%d raw=%d activeLow=%d gpioReliable=%d dvdt=%s final=%s", (int)kChargeStatusPin, lv, (int)g_chargeState.activeLow, (int)useGpio, g_fallbackCharging ? "+" : "-", chargingFinal ? "yes" : "no"); );
  } else {
    // No GPIO routed: rely solely on slope-based fallback
    oled.setBatteryCharging(g_fallbackCharging);
    LOG_EVERY_MS(5000, LOGD("batt", "no charge pin; dvdt=%s", g_fallbackCharging ? "+" : "-"); );
  }
  oled.tick(state.nowMs);
}

static void taskLoRa(AppState &state) {
  lora.tick(state.nowMs);
  // Service event-loop more aggressively to avoid missing interrupts
  Radio.IrqProcess();
}

// ===== Arduino Lifecycle =====
void setup() {
  Serial.begin(115200);
  delay(200);
  Logger::printf(Logger::Level::Info, "boot", "Remote node starting...");

  // ADC setup (ESP32)
  // On ESP32, analogRead returns 0..4095 by default. Some pins require selecting attenuation for full range.
  // Keep defaults for simplicity; users can tune via analogSetPinAttenuation if needed.
  pinMode(ANALOG_INPUT_PIN, INPUT);

  // Display init (optional)
  oled.begin(ENABLE_OLED_DISPLAY);
  oled.setDeviceId(DEVICE_ID);
  oled.setHomescreenRenderer(renderHome, &appState);
  oled.setHeaderRightMode(HeaderRightMode::SignalBars);
  debugRouter.begin(true, &oled, DEVICE_ID);
  // Disable serial output from DebugRouter to avoid duplicate console spam
  debugRouter.setSerialEnabled(false);
  Logger::begin(true, &oled, DEVICE_ID);
  Logger::setLevel(Logger::Level::Info);
  Logger::setVerbose(false);

  // Battery config for this board
  // Force Heltec V3 configuration (from board_config.h)
  g_battCfg.adcPin = BATTERY_ADC_PIN;      // VBAT_ADC
  g_battCfg.ctrlPin = VBAT_CTRL;           // VBAT_CTRL (active-low enable)
  g_battCfg.samples = 12;    // More samples for stable readings
  g_battCfg.useHeltecV3Scaling = true; // Use empirical scaling factor
  g_battCfg.setAttenuationOnFirstRead = true; // Set 11dB attenuation for higher voltage range
  g_battCfg.voltageEmpty = 3.04f; // align with mapVoltageToPercent curve
  g_battCfg.voltageFull = 4.26f;  // align with mapVoltageToPercent curve
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db); // Explicitly set attenuation for VBAT_ADC
  LOGI("batt", "Heltec V3 config applied (adcPin=1, ctrlPin=37, raw/238.7)");

  // Configure charge status pin
  if (kChargeStatusPin >= 0) {
    // Prefer INPUT; optionally enable internal pull-up only if active-low and line floats
    pinMode((uint8_t)kChargeStatusPin, INPUT);
    const int initLv = digitalRead((uint8_t)kChargeStatusPin);
    g_chargeState.activeLow = kChargeActiveLow;
    g_chargeState.isChargingStable = (kChargeActiveLow ? (initLv == LOW) : (initLv == HIGH));
    g_chargeState.lowStreak = g_chargeState.isChargingStable ? 2 : 0;
    g_chargeState.highStreak = g_chargeState.isChargingStable ? 0 : 2;
    g_chargeState.lastChangeMs = millis();
    LOGI("batt", "charge status pin=%d init_raw=%d charging=%s", (int)kChargeStatusPin, initLv, g_chargeState.isChargingStable ? "yes" : "no");
  } else {
    LOGW("batt", "charge status pin not configured");
  }

  // Probe I2C/display presence and print diagnostics
  if (ENABLE_OLED_DISPLAY) {
    // Ensure I2C bus speed is set (optional)
    oled.setI2cClock(500000);
    bool found = oled.probeI2C(OLED_I2C_ADDR);
    LOGI("disp", "probe 0x%02X found=%s", OLED_I2C_ADDR, found ? "yes" : "no");
    if (!found) {
      LOGW("disp", "Tips: check Vext power (LOW=ON), SDA/SCL pins, and address (0x3C vs 0x3D)");
      oled.i2cScan(Serial);
    }
  }

  // Seed PRNG for jitter
  randomSeed((uint32_t)millis() ^ ((uint32_t)DEVICE_ID[0] << 8) ^ ((uint32_t)DEVICE_ID[1] << 16));

  // Register tasks
  scheduler.registerTask("heartbeat", taskHeartbeat, HEARTBEAT_INTERVAL_MS);
  scheduler.registerTask("analog_read", taskReadAnalog, ANALOG_READ_INTERVAL_MS);
  scheduler.registerTask("report", taskReport, 100); // internal jitter decides actual cadence
  scheduler.registerTask("display", taskDisplay, 200); // ~5 FPS
  scheduler.registerTask("lora", taskLoRa, 50);

  // LoRa init (Slave mode)
  // Parse DEVICE_ID to numeric self id (e.g., "03" -> 3)
  uint8_t selfId = (uint8_t)strtoul(DEVICE_ID, nullptr, 10);
  if (selfId == 0) selfId = 1; // avoid zero
  lora.begin(LoRaComm::Mode::Slave, selfId);
  lora.setOnDataReceived(onLoraData);
  lora.setOnAckReceived(onLoraAck);
  lora.setAutoPingEnabled(false); // rely on ACKed telemetry for liveness
  lora.setVerbose(false);
  lora.setLogLevel((uint8_t)Logger::Level::Info);

  LOGI("boot", "slave id=%u rf=%lu Hz", selfId, (unsigned long)LORA_COMM_RF_FREQUENCY);
  LOGI("boot", "Tasks registered.");
  // Initialize first jittered report time
  const int32_t jitter = (int32_t)((int32_t)REPORT_INTERVAL_MS / 5);
  const int32_t delta = (int32_t)REPORT_INTERVAL_MS + (int32_t)random(-jitter, jitter + 1);
  const int32_t safeDelta2 = (delta < 100) ? 100 : delta;
  appState.nextReportDueMs = millis() + (uint32_t)safeDelta2;
}

void loop() {
  appState.nowMs = millis();
  scheduler.tick(appState);
  // Yield to WiFi/BLE stacks if enabled; cheap delay to avoid tight spinning
  delay(1);
}


