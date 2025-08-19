// Minimal, extendable coordination logic for Heltec/ESP32 remote node
// - Uses shared scheduler from lib/scheduler.h
// - Device-specific AppState remains local to this file
// - Example tasks: heartbeat, analog input reader, periodic reporter

#include <Arduino.h>
#include "../lib/scheduler.h"
#include "../lib/display.h"
#include "../lib/debug.h"
#include "../lib/logger.h"
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
};

static AppState appState;
static TaskScheduler<AppState, 8> scheduler;
static OledDisplay oled;
static DebugRouter debugRouter;
static LoRaComm lora;
static volatile uint8_t g_lastAckSrc = 0;

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
    // Serial renderer
    [](Print &out, void *ctx) {
      AppState *s = static_cast<AppState*>(ctx);
      out.print(F("analog raw="));
      out.print(s->analogRaw);
      out.print(F(" V="));
      out.print(s->analogVoltage, 3);
    },
    &appState,
    state.nowMs,
    400
  );
}

static void taskReport(AppState &state) {
  // Minimal key=value CSV line as per docs
  Serial.print(F("id="));
  Serial.print(DEVICE_ID);
  Serial.print(F(",ain_raw="));
  Serial.print(state.analogRaw);
  Serial.print(F(",ain_v="));
  Serial.println(state.analogVoltage, 3);

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

  // LoRa: send a tiny heartbeat message to master (optional in addition to ping)
  // Send heartbeat "hb" to master; require ACK to update link status
  const char hb[] = "hb";
  const bool queued = lora.sendData(MASTER_NODE_ID, (const uint8_t*)hb, (uint8_t)(sizeof(hb) - 1), /*requireAck=*/true);
  if (!queued) {
    Serial.println(F("[lora] warn: heartbeat not queued (outbox full)"));
  }
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
  Serial.println();
  Serial.println(F("[boot] Remote node starting..."));

  // ADC setup (ESP32)
  // On ESP32, analogRead returns 0..4095 by default. Some pins require selecting attenuation for full range.
  // Keep defaults for simplicity; users can tune via analogSetPinAttenuation if needed.
  pinMode(ANALOG_INPUT_PIN, INPUT);

  // Display init (optional)
  oled.begin(ENABLE_OLED_DISPLAY);
  oled.setDeviceId(DEVICE_ID);
  oled.setHomescreenRenderer(renderHome, &appState);
  debugRouter.begin(true, &oled, DEVICE_ID);
  Logger::begin(true, &oled, DEVICE_ID);
  Logger::setLevel(Logger::Level::Info);
  Logger::setVerbose(false);

  // Probe I2C/display presence and print diagnostics
  if (ENABLE_OLED_DISPLAY) {
    // Ensure I2C bus speed is set (optional)
    oled.setI2cClock(500000);
    bool found = oled.probeI2C(OLED_I2C_ADDR);
    Serial.print(F("[display] probe 0x"));
    Serial.print(OLED_I2C_ADDR, HEX);
    Serial.print(F(" found="));
    Serial.println(found ? F("yes") : F("no"));
    if (!found) {
      Serial.println(F("[display] Tips: check Vext power (LOW=ON), SDA/SCL pins, and address (0x3C vs 0x3D)"));
      oled.i2cScan(Serial);
    }
  }

  // Register tasks
  scheduler.registerTask("heartbeat", taskHeartbeat, HEARTBEAT_INTERVAL_MS);
  scheduler.registerTask("analog_read", taskReadAnalog, ANALOG_READ_INTERVAL_MS);
  scheduler.registerTask("report", taskReport, REPORT_INTERVAL_MS);
  scheduler.registerTask("display", taskDisplay, 1000); // ~5 FPS
  scheduler.registerTask("lora", taskLoRa, 50);

  // LoRa init (Slave mode)
  // Parse DEVICE_ID to numeric self id (e.g., "03" -> 3)
  uint8_t selfId = (uint8_t)strtoul(DEVICE_ID, nullptr, 10);
  if (selfId == 0) selfId = 1; // avoid zero
  lora.begin(LoRaComm::Mode::Slave, selfId);
  lora.setOnDataReceived(onLoraData);
  lora.setOnAckReceived(onLoraAck);
  lora.setVerbose(false);
  lora.setLogLevel((uint8_t)Logger::Level::Info);

  Serial.print(F("[boot] slave id="));
  Serial.print(selfId);
  Serial.print(F(" rf="));
  Serial.print((uint32_t)LORA_COMM_RF_FREQUENCY);
  Serial.println(F(" Hz"));

  Serial.println(F("[boot] Tasks registered."));
}

void loop() {
  appState.nowMs = millis();
  scheduler.tick(appState);
  // Yield to WiFi/BLE stacks if enabled; cheap delay to avoid tight spinning
  delay(1);
}


