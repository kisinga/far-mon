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

// ===== LoRa PHY Configuration =====
static const uint32_t RF_FREQUENCY = 868000000UL; // Hz
static const int8_t TX_OUTPUT_POWER = 14;         // dBm
static const uint8_t LORA_BANDWIDTH = 0;          // 125 kHz
static const uint8_t LORA_SPREADING_FACTOR = 7;   // SF7
static const uint8_t LORA_CODING_RATE = 1;        // 4/5
static const uint16_t LORA_PREAMBLE_LENGTH = 8;   // symbols
static const uint8_t LORA_IQ_INVERT = false;      // std IQ

// ===== Protocol Framing =====
// [VER][TYPE][FLAGS][SRC][DST][MSGID_H][MSGID_L][PAYLOAD...]
static const uint8_t PROTO_VER = 1;
static const uint8_t HDR_SIZE = 7;

enum FrameType : uint8_t { FT_DATA = 0x01, FT_ACK = 0x02, FT_PING = 0x03 };

// ===== Master Identity =====
static const uint8_t MASTER_ID = 0x01; // numeric id for frames
static const char DEVICE_ID[] = "01"; // for display/debug

// ===== App Config =====
static const uint32_t PEER_TTL_MS = 15000;
static const uint16_t ACK_TIMEOUT_MS = 1500;
static const uint8_t MAX_RETRIES = 4;
static const uint8_t MAX_PAYLOAD = 64;
static const uint8_t OUTBOX_MAX = 8;
static const uint8_t MAX_PEERS = 16;

// ===== App State =====
struct PeerInfo {
  uint8_t id;
  uint32_t lastSeenMs;
  bool connected;
  int16_t lastRssi;
};

struct AppState {
  uint32_t nowMs = 0;
  bool heartbeatOn = false;
};

static AppState app;
static TaskScheduler<AppState, 8> scheduler;
static OledDisplay oled;
static DebugRouter debugRouter;

// ===== Outbox for retries =====
struct OutMsg {
  bool inUse;
  FrameType type;
  uint8_t destId;
  uint16_t msgId;
  bool requireAck;
  uint8_t attempts;
  uint32_t nextAttemptMs;
  uint8_t len;
  uint8_t buf[MAX_PAYLOAD];
};

static OutMsg outbox[OUTBOX_MAX];
static uint8_t outboxCount = 0;
static uint16_t nextMsgId = 1;
static volatile bool rxPendingAck = false;
static volatile uint8_t rxAckTargetId = 0;
static volatile uint16_t rxAckMessageId = 0;
static int16_t lastRssiDbm = 0;

static PeerInfo peers[MAX_PEERS];

// ===== Radio Events =====
static RadioEvents_t RadioEvents;

// ===== Helpers =====
static uint16_t allocMsgId() {
  if (nextMsgId == 0) nextMsgId = 1;
  return nextMsgId++;
}

static uint8_t buildFrame(uint8_t *out, FrameType type, uint8_t src, uint8_t dst, uint16_t msgId,
                          const uint8_t *payload, uint8_t length) {
  out[0] = PROTO_VER;
  out[1] = (uint8_t)type;
  out[2] = 0; // flags
  out[3] = src;
  out[4] = dst;
  out[5] = (uint8_t)((msgId >> 8) & 0xFF);
  out[6] = (uint8_t)(msgId & 0xFF);
  if (payload != nullptr && length > 0) {
    memcpy(out + HDR_SIZE, payload, length);
  }
  return (uint8_t)(HDR_SIZE + length);
}

static void sendFrame(uint8_t *frame, uint8_t length) {
  Radio.Send(frame, length);
}

static bool enqueueData(uint8_t destId, const uint8_t *payload, uint8_t length, bool requireAck) {
  if (length > MAX_PAYLOAD - HDR_SIZE) return false;
  if (outboxCount >= OUTBOX_MAX) return false;
  OutMsg &m = outbox[outboxCount++];
  m.inUse = true;
  m.type = FT_DATA;
  m.destId = destId;
  m.msgId = allocMsgId();
  m.requireAck = requireAck;
  m.attempts = 0;
  m.nextAttemptMs = 0;
  m.len = buildFrame(m.buf, FT_DATA, MASTER_ID, destId, m.msgId, payload, length);
  return true;
}

static void notePeerSeen(uint8_t id, uint32_t nowMs, int16_t rssi) {
  if (id == 0) return;
  for (uint8_t i = 0; i < MAX_PEERS; i++) {
    if (peers[i].id == id) {
      peers[i].lastSeenMs = nowMs;
      peers[i].connected = true;
      peers[i].lastRssi = rssi;
      return;
    }
  }
  for (uint8_t i = 0; i < MAX_PEERS; i++) {
    if (peers[i].id == 0) {
      peers[i].id = id;
      peers[i].lastSeenMs = nowMs;
      peers[i].connected = true;
      peers[i].lastRssi = rssi;
      return;
    }
  }
  // Overwrite stalest
  uint8_t idx = 0;
  uint32_t oldest = peers[0].lastSeenMs;
  for (uint8_t i = 1; i < MAX_PEERS; i++) {
    if (peers[i].lastSeenMs < oldest) { oldest = peers[i].lastSeenMs; idx = i; }
  }
  peers[idx].id = id;
  peers[idx].lastSeenMs = nowMs;
  peers[idx].connected = true;
  peers[idx].lastRssi = rssi;
}

static void compactOutbox() {
  OutMsg tmp[OUTBOX_MAX];
  uint8_t n = 0;
  for (uint8_t i = 0; i < outboxCount; i++) {
    if (!outbox[i].inUse) continue;
    if (outbox[i].requireAck && outbox[i].attempts >= MAX_RETRIES && (int32_t)(millis() - outbox[i].nextAttemptMs) >= 0) {
      continue; // drop
    }
    tmp[n++] = outbox[i];
  }
  memcpy(outbox, tmp, sizeof(OutMsg) * n);
  outboxCount = n;
}

static int selectNextOutboxIndex(uint32_t nowMs) {
  for (int i = 0; i < (int)outboxCount; i++) {
    OutMsg &m = outbox[i];
    if (!m.inUse) continue;
    if (m.attempts == 0) return i;
    if (m.requireAck && (int32_t)(nowMs - m.nextAttemptMs) >= 0 && m.attempts < MAX_RETRIES) return i;
  }
  return -1;
}

static void removeOutboxByMsgId(uint16_t msgId) {
  for (uint8_t i = 0; i < outboxCount; i++) {
    if (outbox[i].inUse && outbox[i].msgId == msgId) outbox[i].inUse = false;
  }
  compactOutbox();
}

// ===== Radio callbacks =====
static void OnTxDone() {
  Serial.println(F("[tx] done"));
  Radio.Rx(0);
}

static void OnTxTimeout() {
  Serial.println(F("[tx] timeout"));
  Radio.Sleep();
  Radio.Rx(0);
}

static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  (void)snr;
  lastRssiDbm = rssi;
  Radio.Sleep();

  if (size < HDR_SIZE) {
    Radio.Rx(0);
    return;
  }

  const uint8_t ver = payload[0];
  const uint8_t type = payload[1];
  const uint8_t flags = payload[2];
  const uint8_t src = payload[3];
  const uint8_t dst = payload[4];
  const uint16_t msgId = ((uint16_t)payload[5] << 8) | payload[6];
  const uint8_t appLen = (uint8_t)(size - HDR_SIZE);
  (void)flags;

  if (ver != PROTO_VER) {
    Radio.Rx(0);
    return;
  }
  if (!(dst == 0xFF || dst == MASTER_ID)) {
    Radio.Rx(0);
    return;
  }

  // Track peers on any frame
  notePeerSeen(src, millis(), rssi);

  switch (type) {
    case FT_ACK: {
      removeOutboxByMsgId(msgId);
      break;
    }
    case FT_DATA: {
      // schedule ACK
      rxAckTargetId = src;
      rxAckMessageId = msgId;
      rxPendingAck = true;
      // Debug print payload
      Serial.print(F("[rx] DATA from "));
      Serial.print(src);
      Serial.print(F(" len="));
      Serial.print(appLen);
      Serial.print(F(" rssi="));
      Serial.print(rssi);
      Serial.print(F(" payload="));
      if (appLen > 0) {
        Serial.write(payload + HDR_SIZE, appLen);
      }
      Serial.println();
      // Respond with a small DATA if received a heartbeat "hb"
      if (appLen == 2 && payload[HDR_SIZE] == 'h' && payload[HDR_SIZE + 1] == 'b') {
        const char resp[] = "ok";
        (void)enqueueData(src, (const uint8_t*)resp, (uint8_t)(sizeof(resp) - 1), /*requireAck=*/true);
      }
      break;
    }
    case FT_PING: {
      // schedule ACK
      rxAckTargetId = src;
      rxAckMessageId = msgId;
      rxPendingAck = true;
      Serial.print(F("[rx] PING from "));
      Serial.print(src);
      Serial.print(F(" rssi="));
      Serial.println(rssi);
      break;
    }
    default:
      break;
  }

  Radio.Rx(0);
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
  // The relay tracks last RSSI from received frames but is always "connected" as master
  oled.setLoraStatus(true, lastRssiDbm);
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
  // Count connected peers
  uint8_t count = 0;
  uint32_t now = millis();
  for (uint8_t i = 0; i < MAX_PEERS; i++) {
    if (peers[i].id != 0) {
      bool alive = ((int32_t)(now - peers[i].lastSeenMs) < (int32_t)PEER_TTL_MS);
      peers[i].connected = alive;
      if (alive) count++;
    }
  }
  d.drawString(cx, cy + 12, String(count));
  d.setFont(ArialMT_Plain_10);
  // Show first two peers
  uint8_t shown = 0;
  for (uint8_t i = 0; i < MAX_PEERS && shown < 2; i++) {
    if (peers[i].id == 0) continue;
    bool alive = (int32_t)(now - peers[i].lastSeenMs) < (int32_t)PEER_TTL_MS;
    d.drawString(cx, cy + 28 + shown * 10, String("id=") + String(peers[i].id) + String(alive ? " ok" : " x"));
    shown++;
  }
}

static void taskLoRa(AppState &state) {
  (void)state;
  // process radio IRQs
  Radio.IrqProcess();
  // Service again to avoid missed transitions
  Radio.IrqProcess();

  // Update TTLs
  uint32_t now = millis();
  for (uint8_t i = 0; i < MAX_PEERS; i++) {
    if (peers[i].id == 0) continue;
    peers[i].connected = ((int32_t)(now - peers[i].lastSeenMs) < (int32_t)PEER_TTL_MS);
  }

  // Send pending ACK first
  if (rxPendingAck) {
    uint8_t frame[HDR_SIZE];
    uint8_t len = buildFrame(frame, FT_ACK, MASTER_ID, rxAckTargetId, rxAckMessageId, nullptr, 0);
    Serial.print(F("[tx] ACK to "));
    Serial.print(rxAckTargetId);
    Serial.print(F(" msgId="));
    Serial.println(rxAckMessageId);
    sendFrame(frame, len);
    rxPendingAck = false;
    return; // wait for tx done
  }

  // Retry due messages
  int idx = selectNextOutboxIndex(now);
  if (idx >= 0) {
    OutMsg &m = outbox[idx];
    m.attempts++;
    if (m.requireAck) m.nextAttemptMs = now + ACK_TIMEOUT_MS;
    Serial.print(F("[tx] DATA to "));
    Serial.print(m.destId);
    Serial.print(F(" msgId="));
    Serial.println(m.msgId);
    sendFrame(m.buf, m.len);
  }

  compactOutbox();
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
  debugRouter.begin(true, &oled, DEVICE_ID);
  Logger::begin(true, &oled, DEVICE_ID);
  Logger::setLevel(Logger::Level::Info);
  Logger::setVerbose(false);

  // LoRa radio init
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxDone = OnRxDone;
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODING_RATE,
                    LORA_PREAMBLE_LENGTH, false,
                    true, 0, 0, LORA_IQ_INVERT, 3000);
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODING_RATE, 0, LORA_PREAMBLE_LENGTH,
                    0, false, 0, true, 0, 0, LORA_IQ_INVERT, true);
  Radio.Rx(0);

  // Tasks
  scheduler.registerTask("heartbeat", taskHeartbeat, 1000);
  scheduler.registerTask("display", taskDisplay, 800);
  scheduler.registerTask("lora", taskLoRa, 50);
  // scheduler.registerTask("broadcast", maybeBroadcastTick, 10000);

  Serial.println(F("[boot] Master tasks registered."));
  Serial.print(F("[boot] RF="));
  Serial.print(RF_FREQUENCY);
  Serial.print(F(" Hz tx="));
  Serial.print(TX_OUTPUT_POWER);
  Serial.println(F(" dBm"));
}

void loop() {
  app.nowMs = millis();
  scheduler.tick(app);
  delay(1);
}


