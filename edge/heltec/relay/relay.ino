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
  oled.setBatteryStatus(haveBatt, haveBatt ? bp : 0);
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
#else
  g_battCfg.adcPin = 0xFF; // disabled
#endif

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


