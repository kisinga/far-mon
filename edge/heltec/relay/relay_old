// Minimal master (relay) node for Heltec/ESP32
// - Reuses shared scheduler, display, debug patterns
// - LoRa master that tracks slaves via periodic pings
// - ACKs every incoming message
// - Maintains a small retry queue for reliability

#include <Arduino.h>
#include "../lib/scheduler.h"
#include "../lib/display.h"
#include "../lib/display_provider.h"
#include "../lib/wifi_manager.h"
#include "../lib/wifi_config.h"
#include "../lib/debug.h"
#include "../lib/logger.h"
#include "LoRaWan_APP.h"
#include "../lib/lora_comm.h"
#include "../lib/board_config.h"
// System Initialization Helper
#include "../lib/system_init.h"

// ===== Master Identity =====
static const uint8_t MASTER_ID = 0x01; // numeric id for frames
static const char DEVICE_ID[] = "01"; // for display/debug

// ===== App Config =====
static const uint8_t MAX_PEERS = 16;

// WiFi configuration is now centralized in wifi_config.h

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

// WiFi and Display Management (SOLID/KISS/DRY architecture)
static WifiManager wifiManager(wifiConfig);
static DisplayManager displayManager(oled);
static std::unique_ptr<HeaderRightProvider> wifiStatusProvider;

// ===== Optional Battery Reading (modularized) =====
#include "../lib/battery_monitor.h"
static BatteryMonitor::Config g_battCfg;
static BatteryMonitor::BatteryMonitor g_batteryMonitor(g_battCfg);
// Optional charge status pin and polarity from board_config
#ifdef CHARGE_STATUS_PIN
static const int8_t kChargeStatusPin = CHARGE_STATUS_PIN;
#else
static const int8_t kChargeStatusPin = -1;
#endif
#ifdef CHARGE_STATUS_ACTIVE_LOW
static const bool kChargeActiveLow = (CHARGE_STATUS_ACTIVE_LOW != 0);
#else
static const bool kChargeActiveLow = true; // default assumption
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
  debugRouter.debugFor(
    [](SSD1306Wire &d, void *ctx) {
      (void)ctx;
      int16_t cx, cy, cw, ch;
      oled.getContentArea(cx, cy, cw, ch);
      d.setTextAlignment(TEXT_ALIGN_LEFT);
      d.drawString(cx, cy, F("Master"));
      d.drawString(cx, cy + 14, F("Heartbeat"));
    },
    nullptr,
    nullptr, // No serial renderer
    nullptr,
    600
  );
}

static void taskDisplay(AppState &state) {
  // Update display manager (handles all display providers automatically)
  // WiFi status is updated by the dedicated taskWiFi function
  displayManager.updateAndRefresh();

  // Battery management (unchanged - still handled directly on OLED)
  uint8_t bp = 255;
  bool haveBatt = g_batteryMonitor.readPercent(bp);

  // Debug battery readings
  bool ok = false;
  uint16_t vBatMv = g_batteryMonitor.readBatteryMilliVolts(ok);
  if (ok) {
    LOG_EVERY_MS(5000, LOGI("batt", "voltage=%.2fV percent=%u", vBatMv / 1000.0f, (unsigned)bp); );
  } else {
    LOG_EVERY_MS(10000, LOGW("batt", "battery read failed"); );
  }

  oled.setBatteryStatus(haveBatt, haveBatt ? bp : 0);
  g_batteryMonitor.updateChargeStatus(state.nowMs);
  oled.setBatteryCharging(g_batteryMonitor.isCharging());
  LOG_EVERY_MS(5000, LOGD("batt", "final charging status = %s", g_batteryMonitor.isCharging() ? "yes" : "no"); );

  // WiFi status debugging
  LOG_EVERY_MS(10000, wifiManager.printStatus(); );
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

static void taskWiFi(AppState &state) {
  // WiFi manager handles reconnection and status monitoring
  // The display manager will automatically update the display with WiFi status
  wifiManager.update(state.nowMs);
}

// Example: enqueue a small broadcast every 10s (disabled by default)
static void maybeBroadcastTick(AppState &state) {
  (void)state;
  // Disabled to keep minimal; enable if needed for testing
}

// ===== Arduino lifecycle =====
void setup() {
  SystemObjects sys = {
    .oled = oled,
    .debugRouter = debugRouter,
    .lora = lora,
    .batteryMonitor = g_batteryMonitor,
    .batteryConfig = g_battCfg,
    // .scheduler = scheduler, // AppState is generic so this is removed
  };

  initializeSystem(sys, DEVICE_ID, true, MASTER_ID, renderHome, &app);

  // LoRa callbacks are specific to relay.ino
  lora.setOnDataReceived(onLoraData);
  lora.setOnAckReceived(onLoraAck);

  // Initialize WiFi and Display Management (SOLID/KISS/DRY)
  wifiManager.begin();
  wifiStatusProvider = std::make_unique<WifiStatusProvider>(wifiManager);
  displayManager.setHeaderRightProvider(std::move(wifiStatusProvider));

  // Tasks
  scheduler.registerTask("heartbeat", taskHeartbeat, 1000);
  scheduler.registerTask("wifi", taskWiFi, 100); // Frequent WiFi monitoring
  scheduler.registerTask("display", taskDisplay, 800);
  scheduler.registerTask("lora", taskLoRa, 50);
  // scheduler.registerTask("broadcast", maybeBroadcastTick, 10000);

  Serial.println(F("[boot] Master tasks registered with WiFi display provider."));
}

void loop() {
  app.nowMs = millis();
  scheduler.tick(app);
  delay(1);
}


