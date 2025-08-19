// Header-only LoRa communication utility for Heltec ESP32 (SX127x)
// - Reliable send with ACK and retry queue
// - Conditional ACKs (only when requested by sender)
// - Master/Slave modes
//   - Master tracks peers by any received frame (DATA or PING) and TTLs
//   - Slave tracks connectivity based on ACKs for recently ACK-requested messages
// - Non-blocking; call tick(nowMs) regularly (e.g., via scheduler)
// - Extensible framing; decoupled callbacks for received DATA and ACK events

#pragma once

#include <Arduino.h>

// Primary implementation uses Heltec LoRaWAN HAL
// Requires Heltec ESP32 board support package that provides LoRaWan_APP.h
#include "LoRaWan_APP.h"
#include "logger.h"

// Configuration defaults (override by defining before including this header)
// Region selection (override by defining LORA_COMM_RF_FREQUENCY or a region macro before include)
#ifndef LORA_COMM_RF_FREQUENCY
  #if defined(LORA_REGION_US915)
    #define LORA_COMM_RF_FREQUENCY 915000000UL
  #elif defined(LORA_REGION_EU868)
    #define LORA_COMM_RF_FREQUENCY 868000000UL
  #else
    // Default to EU868 if no explicit region selected
    #define LORA_COMM_RF_FREQUENCY 868000000UL
  #endif
#endif

#ifndef LORA_COMM_TX_POWER_DBM
#define LORA_COMM_TX_POWER_DBM 14
#endif

#ifndef LORA_COMM_BANDWIDTH
#define LORA_COMM_BANDWIDTH 0 // 125 kHz
#endif

#ifndef LORA_COMM_SPREADING_FACTOR
#define LORA_COMM_SPREADING_FACTOR 7 // SF7
#endif

#ifndef LORA_COMM_CODING_RATE
#define LORA_COMM_CODING_RATE 1 // 4/5
#endif

#ifndef LORA_COMM_PREAMBLE_LEN
#define LORA_COMM_PREAMBLE_LEN 8
#endif

#ifndef LORA_COMM_SYMBOL_TIMEOUT
#define LORA_COMM_SYMBOL_TIMEOUT 0
#endif

#ifndef LORA_COMM_IQ_INVERT
#define LORA_COMM_IQ_INVERT false
#endif

#ifndef LORA_COMM_MAX_PAYLOAD
#define LORA_COMM_MAX_PAYLOAD 64  // practical safe size at SF7/BW125
#endif

#ifndef LORA_COMM_MAX_OUTBOX
#define LORA_COMM_MAX_OUTBOX 8
#endif

#ifndef LORA_COMM_MAX_PEERS
#define LORA_COMM_MAX_PEERS 16
#endif

#ifndef LORA_COMM_ACK_TIMEOUT_MS
#define LORA_COMM_ACK_TIMEOUT_MS 1500
#endif

#ifndef LORA_COMM_MAX_RETRIES
#define LORA_COMM_MAX_RETRIES 4
#endif

#ifndef LORA_COMM_SLAVE_PING_INTERVAL_MS
#define LORA_COMM_SLAVE_PING_INTERVAL_MS 5000
#endif

#ifndef LORA_COMM_MASTER_TTL_MS
#define LORA_COMM_MASTER_TTL_MS 15000
#endif

// Optional jittered ping window for slaves. If defined, ping cadence is randomized
// between MIN and MAX inclusive. Presence is also piggybacked on DATA uplinks.
#ifndef LORA_COMM_SLAVE_PING_MIN_MS
#define LORA_COMM_SLAVE_PING_MIN_MS 30000
#endif

#ifndef LORA_COMM_SLAVE_PING_MAX_MS
#define LORA_COMM_SLAVE_PING_MAX_MS 120000
#endif

class LoRaComm {
 public:
  enum class Mode : uint8_t { Master = 0, Slave = 1 };

  // Frame types for the internal protocol
  enum class FrameType : uint8_t { Data = 0x01, Ack = 0x02, Ping = 0x03 };

  // Callback when DATA is received
  using OnDataReceived = void (*)(uint8_t srcId, const uint8_t *payload, uint8_t length);

  // Callback when an ACK for our outgoing message is received
  using OnAckReceived = void (*)(uint8_t srcId, uint16_t messageId);

  // Peer info for master view
  struct PeerInfo {
    uint8_t peerId;
    uint32_t lastSeenMs;
    bool connected;
  };

  LoRaComm()
      : mode(Mode::Slave), selfId(0), onDataCb(nullptr), onAckCb(nullptr),
        lastPingSentMs(0), lastAckOkMs(0), lastRadioActivityMs(0), lastNowMs(0),
        lastRssiDbm(0),
        rxPendingAck(false), rxAckTargetId(0), rxAckMessageId(0),
        outboxCount(0), nextMessageId(1), awaitingAckMsgId(0), awaitingAckSrcId(0),
        radioState(State::Idle), autoPingEnabled(true) {
    memset(peers, 0, sizeof(peers));
  }

  // Begin the radio with defaults; set initBoard=false if the app already called Mcu.begin(...)
  void begin(Mode m, uint8_t id, bool initBoard = true) {
    mode = m;
    selfId = id;

    if (initBoard) {
      Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
    }

    radioEvents.TxDone = &HandleTxDone;
    radioEvents.TxTimeout = &HandleTxTimeout;
    radioEvents.RxDone = &HandleRxDone;

    Radio.Init(&radioEvents);
    Radio.SetChannel(LORA_COMM_RF_FREQUENCY);
    Radio.SetTxConfig(MODEM_LORA, LORA_COMM_TX_POWER_DBM, 0, LORA_COMM_BANDWIDTH,
                      LORA_COMM_SPREADING_FACTOR, LORA_COMM_CODING_RATE,
                      LORA_COMM_PREAMBLE_LEN, false,
                      true, 0, 0, LORA_COMM_IQ_INVERT, 3000);

    Radio.SetRxConfig(MODEM_LORA, LORA_COMM_BANDWIDTH, LORA_COMM_SPREADING_FACTOR,
                      LORA_COMM_CODING_RATE, 0, LORA_COMM_PREAMBLE_LEN,
                      LORA_COMM_SYMBOL_TIMEOUT, false,
                      0, true, 0, 0, LORA_COMM_IQ_INVERT, true);

    getInstance() = this;
    enterRxMode();
  }

  void setOnDataReceived(OnDataReceived cb) { onDataCb = cb; }
  void setOnAckReceived(OnAckReceived cb) { onAckCb = cb; }
  // Logging control (defaults: disabled)
  void setVerbose(bool verbose) { verboseEnabled = verbose; }
  void setLogLevel(uint8_t level /*Logger::Level*/) { logLevel = level; }
  void setAutoPingEnabled(bool enabled) { autoPingEnabled = enabled; }

  // Enqueue application DATA. Returns false if outbox is full or payload too large.
  bool sendData(uint8_t destId, const uint8_t *payload, uint8_t length, bool requireAck = true) {
    if (length > maxAppPayload()) return false;
    if (outboxCount >= LORA_COMM_MAX_OUTBOX) return false;

    OutMsg &m = outbox[outboxCount++];
    m.inUse = true;
    m.type = FrameType::Data;
    m.destId = destId;
    m.msgId = allocateMsgId();
    m.requireAck = requireAck;
    m.attempts = 0;
    m.nextAttemptMs = 0;
    const uint8_t flags = requireAck ? kFlagRequireAck : 0;
    m.length = buildFrame(m.buf, FrameType::Data, selfId, destId, m.msgId, payload, length, flags);
    Logger::printf(Logger::Level::Debug, "lora", "ENQ DATA to=%u msgId=%u obx=%u", destId, m.msgId, outboxCount);
    return true;
  }

  // For slaves: opportunistic ping. Master will track presence via received pings.
  bool sendPing() {
    if (outboxCount >= LORA_COMM_MAX_OUTBOX) return false;
    OutMsg &m = outbox[outboxCount++];
    m.inUse = true;
    m.type = FrameType::Ping;
    m.destId = 0xFF; // broadcast or unknown; app may filter by srcId
    m.msgId = allocateMsgId();
    m.requireAck = false; // Pings are not acked; presence tracked by master
    m.attempts = 0;
    m.nextAttemptMs = 0;
    m.length = buildFrame(m.buf, FrameType::Ping, selfId, m.destId, m.msgId, nullptr, 0, /*flags=*/0);
    Logger::printf(Logger::Level::Debug, "lora", "ENQ PING msgId=%u obx=%u", m.msgId, outboxCount);
    return true;
  }

  // Call frequently; processes radio IRQs, sends retries, sends ACKs, updates peer TTLs.
  void tick(uint32_t nowMs) {
    lastNowMs = nowMs;
    Radio.IrqProcess();

    // Automatic slave ping (jittered and infrequent)
    if (mode == Mode::Slave && autoPingEnabled) {
      if ((int32_t)(nowMs - lastPingSentMs) >= 0) {
        (void)sendPing();
        const uint32_t window = (uint32_t)random((long)LORA_COMM_SLAVE_PING_MIN_MS, (long)LORA_COMM_SLAVE_PING_MAX_MS + 1);
        lastPingSentMs = nowMs + window;
      }
    } else {
      // Update master peer TTLs
      for (size_t i = 0; i < LORA_COMM_MAX_PEERS; i++) {
        if (peers[i].peerId != 0) {
          bool alive = ((int32_t)(nowMs - peers[i].lastSeenMs) < (int32_t)LORA_COMM_MASTER_TTL_MS);
          peers[i].connected = alive;
        }
      }
    }

    // Priority 1: send pending ACK as soon as radio is idle
    if (rxPendingAck && radioState != State::Tx) {
      uint8_t frame[8];
      uint8_t len = buildFrame(frame, FrameType::Ack, selfId, rxAckTargetId, rxAckMessageId, nullptr, 0, /*flags=*/0);
      if (Logger::isEnabled(verboseEnabled ? Logger::Level::Verbose : (Logger::Level)logLevel)) {
        Logger::printf(Logger::Level::Debug, "lora", "TX ACK to=%u msgId=%u", rxAckTargetId, rxAckMessageId);
      }
      sendFrame(frame, len);
      rxPendingAck = false;
      return; // wait for TX done
    }

    // Priority 2: transmit or retry queued messages
    if (radioState != State::Tx) {
      int idx = selectNextOutboxIndex(nowMs);
      if (idx >= 0) {
        OutMsg &m = outbox[idx];
        m.attempts++;
        if (m.requireAck) {
          m.nextAttemptMs = nowMs + LORA_COMM_ACK_TIMEOUT_MS;
          awaitingAckMsgId = m.msgId;
          awaitingAckSrcId = 0; // not used; we match by msgId only
        }
        Logger::printf(Logger::Level::Debug, "lora", "TX %s to=%u msgId=%u%s",
                       (m.type == FrameType::Data ? "DATA" : "PING"), m.destId, m.msgId,
                       (m.requireAck ? " waitAck" : ""));
        sendFrame(m.buf, m.length);
        return;
      }
      if (outboxCount > 0) {
        Logger::printf(Logger::Level::Warn, "lora", "stall rs=%d obx=%u", (int)radioState, outboxCount);
      }
    }

    // Cleanup: drop expired messages
    compactOutbox();
  }

  // Slave connectivity view
  bool isConnected() const {
    if (mode == Mode::Master) return true;
    return lastAckOkMs != 0 && ((int32_t)(lastNowMs - lastAckOkMs) < (int32_t)(LORA_COMM_MASTER_TTL_MS));
  }

  int16_t getLastRssiDbm() const { return lastRssiDbm; }

  // Master APIs
  size_t getPeerCount() const {
    size_t c = 0;
    for (size_t i = 0; i < LORA_COMM_MAX_PEERS; i++) if (peers[i].peerId != 0) c++;
    return c;
  }

  bool getPeerByIndex(size_t index, PeerInfo &out) const {
    size_t seen = 0;
    for (size_t i = 0; i < LORA_COMM_MAX_PEERS; i++) {
      if (peers[i].peerId != 0) {
        if (seen == index) { out = peers[i]; return true; }
        seen++;
      }
    }
    return false;
  }

 private:
  // Internal state
  enum class State : uint8_t { Idle = 0, Rx = 1, Tx = 2 };

  struct OutMsg {
    bool inUse;
    FrameType type;
    uint8_t destId;
    uint16_t msgId;
    bool requireAck;
    uint8_t attempts;
    uint32_t nextAttemptMs;
    uint8_t length;
    uint8_t buf[LORA_COMM_MAX_PAYLOAD];
  };

  // Framing: [VER=1][TYPE][FLAGS=0][SRC][DST][MSGID_H][MSGID_L][PAYLOAD...]
  static constexpr uint8_t kProtocolVersion = 1;
  static constexpr uint8_t kHeaderSize = 7;
  // Flags bitfield
  static constexpr uint8_t kFlagRequireAck = 0x01; // when set on DATA, receiver must ACK

  static LoRaComm *&getInstance() {
    static LoRaComm *inst = nullptr;
    return inst;
  }

  Mode mode;
  uint8_t selfId;
  OnDataReceived onDataCb;
  OnAckReceived onAckCb;
  uint32_t lastPingSentMs;
  uint32_t lastAckOkMs;
  uint32_t lastRadioActivityMs;
  uint32_t lastNowMs;
  int16_t lastRssiDbm;

  volatile bool rxPendingAck;
  volatile uint8_t rxAckTargetId;
  volatile uint16_t rxAckMessageId;

  OutMsg outbox[LORA_COMM_MAX_OUTBOX];
  uint8_t outboxCount;
  uint16_t nextMessageId;
  uint16_t awaitingAckMsgId;
  uint8_t awaitingAckSrcId;

  PeerInfo peers[LORA_COMM_MAX_PEERS];

  RadioEvents_t radioEvents;
  State radioState;
  bool autoPingEnabled;

  bool verboseEnabled = false;
  uint8_t logLevel = (uint8_t)Logger::Level::Info;

  static void HandleTxDone() {
    if (getInstance() == nullptr) return;
    getInstance()->onTxDone();
  }

  static void HandleTxTimeout() {
    if (getInstance() == nullptr) return;
    getInstance()->onTxTimeout();
  }

  static void HandleRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
    (void)snr;
    if (getInstance() == nullptr) return;
    getInstance()->onRxDone(payload, size, rssi);
  }

  void onTxDone() {
    lastRadioActivityMs = millis();
    radioState = State::Idle;
    Logger::printf(Logger::Level::Debug, "lora", "TX done");
    enterRxMode();
  }

  void onTxTimeout() {
    lastRadioActivityMs = millis();
    radioState = State::Idle;
    Logger::printf(Logger::Level::Warn, "lora", "TX timeout");
    enterRxMode();
  }

  void onRxDone(uint8_t *payload, uint16_t size, int16_t rssi) {
    lastRadioActivityMs = millis();
    lastRssiDbm = rssi;
    Radio.Sleep();
    radioState = State::Idle;

    if (size < kHeaderSize) {
      enterRxMode();
      return;
    }

    const uint8_t ver = payload[0];
    const FrameType type = (FrameType)payload[1];
    const uint8_t flags = payload[2];
    const uint8_t src = payload[3];
    const uint8_t dst = payload[4];
    const uint16_t msgId = ((uint16_t)payload[5] << 8) | payload[6];
    const uint8_t appLen = (uint8_t)(size - kHeaderSize);
    (void)flags; // reserved

    // Filter destination: accept broadcast (0xFF) or our id
    if (!(dst == 0xFF || dst == selfId)) {
      enterRxMode();
      return;
    }

    if (ver != kProtocolVersion) {
      enterRxMode();
      return;
    }

    // Track peers (master) and basic presence (any valid frame counts)
    if (mode == Mode::Master) {
      notePeerSeen(src, millis());
    }

    switch (type) {
      case FrameType::Ack: {
        // ACK for our outgoing message
        if (onAckCb != nullptr) onAckCb(src, msgId);
        Logger::printf(Logger::Level::Debug, "lora", "RX ACK from=%u msgId=%u", src, msgId);
        if (mode == Mode::Slave) {
          lastAckOkMs = millis();
        }
        // Remove matching outbox entry
        removeOutboxByMsgId(msgId);
        break;
      }
      case FrameType::Data: {
        // Application data; schedule ACK back to source only if requested
        if ((flags & kFlagRequireAck) != 0) {
          rxAckTargetId = src;
          rxAckMessageId = msgId;
          rxPendingAck = true;
        }
        Logger::printf(Logger::Level::Info, "lora", "RX DATA from=%u len=%u", src, appLen);
        if (onDataCb != nullptr && appLen > 0) {
          onDataCb(src, payload + kHeaderSize, appLen);
        }
        break;
      }
      case FrameType::Ping: {
        // Presence only; master marks peer seen in notePeerSeen above
        // Do not ACK pings to conserve airtime
        Logger::printf(Logger::Level::Info, "lora", "RX PING from=%u", src);
        break;
      }
      default:
        break;
    }

    enterRxMode();
  }

  void enterRxMode() {
    Radio.Rx(0); // continuous RX
    radioState = State::Rx;
  }

  uint16_t allocateMsgId() {
    if (nextMessageId == 0) nextMessageId = 1; // avoid zero
    return nextMessageId++;
  }

  uint8_t maxAppPayload() const {
    // Reserve header size
    if (LORA_COMM_MAX_PAYLOAD <= kHeaderSize) return 0;
    return (uint8_t)(LORA_COMM_MAX_PAYLOAD - kHeaderSize);
  }

  uint8_t buildFrame(uint8_t *out, FrameType type, uint8_t src, uint8_t dst,
                     uint16_t msgId, const uint8_t *payload, uint8_t length, uint8_t flags = 0) {
    out[0] = kProtocolVersion;
    out[1] = (uint8_t)type;
    out[2] = flags; // flags
    out[3] = src;
    out[4] = dst;
    out[5] = (uint8_t)((msgId >> 8) & 0xFF);
    out[6] = (uint8_t)(msgId & 0xFF);
    if (payload != nullptr && length > 0) {
      memcpy(out + kHeaderSize, payload, length);
    }
    return (uint8_t)(kHeaderSize + length);
  }

  void sendFrame(uint8_t *frame, uint8_t length) {
    Radio.Send(frame, length);
    radioState = State::Tx;
  }

  int selectNextOutboxIndex(uint32_t nowMs) {
    // Prefer messages that are due for retry or never attempted yet
    for (int i = 0; i < (int)outboxCount; i++) {
      OutMsg &m = outbox[i];
      if (!m.inUse) continue;
      if (m.attempts == 0) return i;
      if (m.requireAck && (int32_t)(nowMs - m.nextAttemptMs) >= 0 && m.attempts < LORA_COMM_MAX_RETRIES) {
        return i;
      }
      if (!m.requireAck && m.attempts == 0) return i;
    }
    return -1;
  }

  void removeOutboxByMsgId(uint16_t msgId) {
    for (size_t i = 0; i < outboxCount; i++) {
      if (outbox[i].inUse && outbox[i].msgId == msgId) {
        outbox[i].inUse = false;
      }
    }
    compactOutbox();
  }

  void compactOutbox() {
    // Remove exhausted retries and pack the array
    OutMsg tmp[LORA_COMM_MAX_OUTBOX];
    uint8_t n = 0;
    for (size_t i = 0; i < outboxCount; i++) {
      OutMsg &m = outbox[i];
      if (!m.inUse) continue;
      if (m.requireAck && m.attempts >= LORA_COMM_MAX_RETRIES && (int32_t)(millis() - m.nextAttemptMs) >= 0) {
        // drop
        Logger::printf(Logger::Level::Warn, "lora", "drop msgId=%u", m.msgId);
        continue;
      }
      tmp[n++] = m;
    }
    memcpy(outbox, tmp, sizeof(OutMsg) * n);
    outboxCount = n;
  }

  void notePeerSeen(uint8_t peerId, uint32_t nowMs) {
    if (peerId == 0) return; // 0 reserved
    for (size_t i = 0; i < LORA_COMM_MAX_PEERS; i++) {
      if (peers[i].peerId == peerId) {
        peers[i].lastSeenMs = nowMs;
        peers[i].connected = true;
        return;
      }
    }
    // Insert into empty slot
    for (size_t i = 0; i < LORA_COMM_MAX_PEERS; i++) {
      if (peers[i].peerId == 0) {
        peers[i].peerId = peerId;
        peers[i].lastSeenMs = nowMs;
        peers[i].connected = true;
        return;
      }
    }
    // No slot: overwrite the stalest
    size_t idx = 0;
    uint32_t oldest = peers[0].lastSeenMs;
    for (size_t i = 1; i < LORA_COMM_MAX_PEERS; i++) {
      if (peers[i].lastSeenMs < oldest) { oldest = peers[i].lastSeenMs; idx = i; }
    }
    peers[idx].peerId = peerId;
    peers[idx].lastSeenMs = nowMs;
    peers[idx].connected = true;
  }
};

// No global inline state; getInstance() holds a function-local static pointer

#if 0
// Usage sketch (example only; do not compile from header):

#include "../lib/lora_comm.h"

static LoRaComm lora;

void onData(uint8_t src, const uint8_t *payload, uint8_t len) {
  // handle data
}

void onAck(uint8_t src, uint16_t msgId) {
  // handle ack
}

void setup() {
  Serial.begin(115200);
  lora.begin(LoRaComm::Mode::Slave, 3);
  lora.setOnDataReceived(onData);
  lora.setOnAckReceived(onAck);
}

void loop() {
  lora.tick(millis());
  delay(1);
}
#endif


