// Minimal master (relay) node for Heltec/ESP32
// - Reuses shared scheduler, display, debug patterns
// - LoRa master that tracks slaves via periodic pings
// - ACKs every incoming message
// - Maintains a small retry queue for reliability

#include <Arduino.h>
#include "../lib/scheduler.h"
#include "../lib/display.h"
#include "../lib/debug.h"
#include "../lib/logger.h"
#include "LoRaWan_APP.h"
#include "../lib/lora_comm.h"

// ===== Master Identity =====
static const uint8_t MASTER_ID = 0x01; // numeric id for frames
static const char DEVICE_ID[] = "01"; // for display/debug

// ===== App Config =====
static const uint8_t MAX_PEERS = 16;

// ===== App State =====
struct AppState {
  uint32_t nowMs = 0;
  bool heartbeatOn = false;
};

static AppState app;
static TaskScheduler<AppState, 8> scheduler;
static OledDisplay oled;
static DebugRouter debugRouter;
static LoRaComm lora;

// ===== Optional Battery Reading (modularized) =====
#include "../lib/battery.h"
static Battery::Config g_battCfg;
// Optional charge status pin: active-low typical for charger STAT
#ifdef CHARGE_STATUS_PIN
static const int8_t kChargeStatusPin = CHARGE_STATUS_PIN;
#else
static const int8_t kChargeStatusPin = -1;
#endif

// Replace custom radio callbacks with LoRaComm event callbacks
static void onLoraData(uint8_t src, const uint8_t *payload, uint8_t len) {
  Serial.print(F("[rx] DATA from "));
  Serial.print(src);
  Serial.print(F(" len="));
  Serial.println(len);
}

static void onLoraAck(uint8_t src, uint16_t msgId) {
  Serial.print(F("[rx] ACK from "));
  Serial.print(src);
  Serial.print(F(" msgId="));
  Serial.println(msgId);
}

// ===== Tasks =====
static void taskHeartbeat(AppState &state) {
  state.heartbeatOn = !state.heartbeatOn;
  debugRouter.debug(
    [](SSD1306Wire &d, void *ctx) {
      (void)ctx;
      int16_t cx, cy, cw, ch;
      oled.getContentArea(cx, cy, cw, ch);
      d.setTextAlignment(TEXT_ALIGN_LEFT);
      d.drawString(cx, cy, F("Master"));
      d.drawString(cx, cy + 14, F("Heartbeat"));
    },
    nullptr,
    [](Print &out, void *ctx) {
      (void)ctx;
      out.print(F("master hb"));
    },
    nullptr,
    state.nowMs,
    600
  );
}

static void taskDisplay(AppState &state) {
  // Header right: show peer count (master)
  size_t connectedCount = 0;
  for (size_t i = 0;; i++) {
    LoRaComm::PeerInfo p{};
    if (!lora.getPeerByIndex(i, p)) break;
    if (p.connected) connectedCount++;
  }
  oled.setHeaderRightMode(HeaderRightMode::PeerCount);
  oled.setPeerCount((uint16_t)connectedCount);

  // LoRa status still provided to allow other renderers to use RSSI if needed
  oled.setLoraStatus(true, lora.getLastRssiDbm());

  // Battery icon on left
  uint8_t bp = 255;
  bool haveBatt = Battery::readPercent(g_battCfg, bp);
  
  // Debug battery readings
  bool ok = false;
  uint16_t vBatMv = Battery::readBatteryMilliVolts(g_battCfg, ok);
  if (ok) {
    Serial.print(F("[battery] voltage="));
    Serial.print(vBatMv / 1000.0f, 2);
    Serial.print(F("V percent="));
    Serial.println(bp);
  } else {
    Serial.println(F("[battery] read failed"));
  }
  
  oled.setBatteryStatus(haveBatt, haveBatt ? bp : 0);
  if (kChargeStatusPin >= 0) {
    int lv = digitalRead((uint8_t)kChargeStatusPin);
    // Assume active-low: 0 = charging
    oled.setBatteryCharging(lv == LOW);
    Serial.print(F("[battery] charging="));
    Serial.println(lv == LOW ? "yes" : "no");
  }
  oled.tick(state.nowMs);
}

static void renderHome(SSD1306Wire &d, void *ctx) {
  (void)ctx;
  // Fit within computed content area
  int16_t cx, cy, cw, ch;
  oled.getContentArea(cx, cy, cw, ch);
  d.setTextAlignment(TEXT_ALIGN_LEFT);
  d.setFont(ArialMT_Plain_10);
  d.drawString(cx, cy, F("Peers"));
  d.setFont(ArialMT_Plain_16);
  // Count only currently connected peers (per TTL in LoRaComm)
  size_t connectedCount = 0;
  for (size_t i = 0; ; i++) {
    LoRaComm::PeerInfo tmp{};
    if (!lora.getPeerByIndex(i, tmp)) break;
    if (tmp.connected) connectedCount++;
  }
  d.drawString(cx, cy + 12, String((uint32_t)connectedCount));
  d.setFont(ArialMT_Plain_10);
  // Show first two peers
  uint8_t shown = 0;
  for (size_t i = 0; i < MAX_PEERS && shown < 2; i++) {
    LoRaComm::PeerInfo p{};
    if (!lora.getPeerByIndex(i, p)) break;
    d.drawString(cx, cy + 28 + shown * 10, String("id=") + String(p.peerId) + String(p.connected ? " ok" : " x"));
    shown++;
  }
}

static void taskLoRa(AppState &state) {
  (void)state;
  lora.tick(millis());
  Radio.IrqProcess();
}

// Example: enqueue a small broadcast every 10s (disabled by default)
static void maybeBroadcastTick(AppState &state) {
  (void)state;
  // Disabled to keep minimal; enable if needed for testing
}

// ===== Arduino lifecycle =====
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("[boot] Master node starting..."));

  // Display init (optional)
  oled.begin(true);
  oled.setDeviceId(DEVICE_ID);
  oled.setHomescreenRenderer(renderHome, &app);
  oled.setHeaderRightMode(HeaderRightMode::PeerCount);
  debugRouter.begin(true, &oled, DEVICE_ID);
  Logger::begin(true, &oled, DEVICE_ID);
  Logger::setLevel(Logger::Level::Info);
  Logger::setVerbose(false);

  // Battery config: set adcPin/divider here if available
#ifdef BATTERY_ADC_PIN
  g_battCfg.adcPin = BATTERY_ADC_PIN;
  // Recommended: 11dB attenuation applied by helper; set divider per schematic
  // Example default 100k:100k
  g_battCfg.dividerRatio = 2.0f;
  g_battCfg.samples = 12;
  Serial.print(F("[battery] adcPin="));
  Serial.print((int)g_battCfg.adcPin);
  Serial.print(F(" ratio="));
  Serial.print(g_battCfg.dividerRatio, 3);
  Serial.print(F(" samples="));
  Serial.println((int)g_battCfg.samples);
#else
  #if defined(ARDUINO_heltec_wifi_32_lora_V3)
    // Heltec WiFi LoRa 32 V3 defaults (from board schematic/known examples)
    g_battCfg.adcPin = 1;      // VBAT_ADC
    g_battCfg.ctrlPin = 37;    // VBAT_CTRL (active-low enable)
    g_battCfg.samples = 12;
    g_battCfg.useHeltecV3Scaling = true; // raw/238.7
    Serial.println(F("[battery] Heltec V3 defaults applied (adcPin=1, ctrlPin=37, raw/238.7)"));
  #else
    g_battCfg.adcPin = 0xFF; // disabled
    Serial.println(F("[battery] disabled: define BATTERY_ADC_PIN to enable VBAT reading"));
  #endif
#endif

  // Configure optional charge status pin (input with pull-up if active-low open-drain)
  if (kChargeStatusPin >= 0) {
    pinMode((uint8_t)kChargeStatusPin, INPUT_PULLUP);
    Serial.print(F("[battery] charge status pin="));
    Serial.println((int)kChargeStatusPin);
  } else {
    Serial.println(F("[battery] no charge status pin configured (define CHARGE_STATUS_PIN if routed)"));
  }

  // LoRa init via LoRaComm
  lora.begin(LoRaComm::Mode::Master, MASTER_ID);
  lora.setOnDataReceived(onLoraData);
  lora.setOnAckReceived(onLoraAck);
  lora.setVerbose(false);
  lora.setLogLevel((uint8_t)Logger::Level::Info);

  // Tasks
  scheduler.registerTask("heartbeat", taskHeartbeat, 1000);
  scheduler.registerTask("display", taskDisplay, 800);
  scheduler.registerTask("lora", taskLoRa, 50);
  // scheduler.registerTask("broadcast", maybeBroadcastTick, 10000);

  Serial.println(F("[boot] Master tasks registered."));
  Serial.print(F("[boot] RF="));
  Serial.print((uint32_t)LORA_COMM_RF_FREQUENCY);
  Serial.print(F(" Hz tx="));
  Serial.print((int)LORA_COMM_TX_POWER_DBM);
  Serial.println(F(" dBm"));
}

void loop() {
  app.nowMs = millis();
  scheduler.tick(app);
  delay(1);
}


