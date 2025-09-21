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
#include <limits>

// Primary implementation uses Heltec LoRaWAN HAL
// Requires Heltec ESP32 board support package that provides LoRaWan_APP.h
#include "LoRaWan_APP.h"
#include "core_logger.h"
#include "common_message_types.h"

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

#ifndef LORA_COMM_TX_GUARD_MS
#define LORA_COMM_TX_GUARD_MS 8000  // Faster recovery if TX completion IRQ is missed
#endif

#ifndef LORA_COMM_TX_STUCK_REINIT_COUNT
#define LORA_COMM_TX_STUCK_REINIT_COUNT 3  // Reinitialize sooner to recover radio
#endif

#ifndef LORA_COMM_CONNECTION_CHECK_MS
#define LORA_COMM_CONNECTION_CHECK_MS 10000  // Check connection every 10 seconds
#endif

#ifndef LORA_COMM_RECONNECT_ATTEMPT_MS
#define LORA_COMM_RECONNECT_ATTEMPT_MS 5000  // Attempt reconnection every 5 seconds when disconnected
#endif

class LoRaComm {
  public:
  enum class Mode : uint8_t { Master = 0, Slave = 1 };

  // Frame types for the internal protocol
  enum class FrameType : uint8_t { Data = 0x01, Ack = 0x02 };

  // Connection states for better reconnection handling
  enum class ConnectionState : uint8_t { Disconnected = 0, Connecting = 1, Connected = 2 };

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
        lastAckOkMs(0), lastRadioActivityMs(0), lastNowMs(0),
        lastRssiDbm(0),
        rxPendingAck(false), rxAckTargetId(0), rxAckMessageId(0),
        outboxCount(0), nextMessageId(1), awaitingAckMsgId(0), awaitingAckSrcId(0),
        radioState(State::Idle), stallActive(false), initialized(false),
        lastConnectionCheckMs(0), nextReconnectAttemptMs(0), masterNodeId(1),
        connectionState(ConnectionState::Disconnected) {
    memset(peers, 0, sizeof(peers));
    lastAckOkMs = 0;
    lastRssiDbm = std::numeric_limits<int16_t>::min();
  }

  // Safe begin that prevents double initialization
  // Returns true if initialization was performed, false if already initialized
  bool safeBegin(Mode m, uint8_t id) {
    if (initialized) {
      return false; // Already initialized
    }
    unsafeBegin(m, id);
    return true;
  }

  // Backwards-compatible begin wrapper used by transports
  void begin(Mode m, uint8_t id) {
    (void)safeBegin(m, id);
  }

  // Expose selected APIs used by services/transports
  void setOnDataReceived(OnDataReceived cb) { onDataCb = cb; }
  void setOnAckReceived(OnAckReceived cb) { onAckCb = cb; }
  void setVerbose(bool verbose) { verboseEnabled = verbose; }
  void setLogLevel(uint8_t level /*Logger::Level*/) { logLevel = level; }
  void setPeerTimeout(uint32_t timeoutMs) { peerTimeoutMs = timeoutMs; }
  void setMasterNodeId(uint8_t masterId) { masterNodeId = masterId; }

  ConnectionState getConnectionState() const { return connectionState; }

  bool sendData(uint8_t destId, const uint8_t *payload, uint8_t length, bool requireAck = true) {
    if (length > maxAppPayload()) return false;
    // Reserve one slot for housekeeping (e.g., PING) to preserve presence
    if (outboxCount >= (uint8_t)(LORA_COMM_MAX_OUTBOX - 1)) return false;

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
  void tick(uint32_t nowMs) {
    lastNowMs = nowMs;
    // Radio.IrqProcess(); // This should be called from the main application loop/task

    // TX watchdog: recover if TX completion IRQ is missed
    if (radioState == State::Tx) {
      if ((int32_t)(nowMs - lastRadioActivityMs) > (int32_t)LORA_COMM_TX_GUARD_MS) {
        LOG_EVERY_MS(1000, { Logger::printf(Logger::Level::Warn, "lora", "TX stuck; forcing RX"); });
        txStuckConsecutive++;
        if (txStuckConsecutive >= LORA_COMM_TX_STUCK_REINIT_COUNT) {
          Logger::printf(Logger::Level::Warn, "lora", "Reinitializing radio after %u stuck events", (unsigned)txStuckConsecutive);
          reinitializeRadio();
          txStuckConsecutive = 0;
        }
        // Treat as a soft timeout for the in-flight message
        if (currentTxMsgId != 0) {
          for (size_t i = 0; i < outboxCount; i++) {
            OutMsg &m = outbox[i];
            if (m.inUse && m.msgId == currentTxMsgId) {
              if (m.requireAck) {
                m.nextAttemptMs = nowMs + LORA_COMM_ACK_TIMEOUT_MS;
              } else {
                // Non-ACK: drop to prevent log spam and backlog
                m.inUse = false;
                statsDropped++;
                Logger::printf(Logger::Level::Warn, "lora", "drop (stuck) msgId=%u", m.msgId);
              }
              break;
            }
          }
          compactOutbox();
          currentTxMsgId = 0;
        }
        Radio.Sleep();
        radioState = State::Idle;
        enterRxMode();
      }
    }
    
    // Update peer TTLs. This is critical for connection state management.
    if (mode == Mode::Master) {
      for (size_t i = 0; i < LORA_COMM_MAX_PEERS; i++) {
        if (peers[i].peerId != 0) {
          bool alive = ((int32_t)(nowMs - peers[i].lastSeenMs) < (int32_t)peerTimeoutMs);
          peers[i].connected = alive;
        }
      }
    } else { // Slave mode
        // Slaves only track the master to help with connection state logic
        for (size_t i = 0; i < LORA_COMM_MAX_PEERS; i++) {
            if (peers[i].peerId == masterNodeId) {
                peers[i].connected = ((int32_t)(nowMs - peers[i].lastSeenMs) < (int32_t)peerTimeoutMs);
                break; // Found master, no need to check others
            }
        }
    }

    updateConnectionState(nowMs);

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
    // In continuous RX mode, we can transmit even when radioState is Rx
    // The radio will automatically switch to TX when sending
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
        statsTx++;
        if (outboxCount > statsOutboxMax) statsOutboxMax = outboxCount;
        Logger::printf(Logger::Level::Debug, "lora", "TX DATA to=%u msgId=%u%s",
                       m.destId, m.msgId,
                       (m.requireAck ? " waitAck" : ""));
        currentTxMsgId = m.msgId;
        sendFrame(m.buf, m.length);
        return;
      }
    }

    // If we reached here, it means we have nothing to send right now.
    // Now check for stall condition.
    if (outboxCount > 0 && radioState != State::Tx) {
      // We have messages but none are ready to send.
      if (stallDetectStartMs == 0) {
        stallDetectStartMs = nowMs;
      }
    
      // A stall is when we have items in the outbox, but nothing is eligible for transmission
      // for a period longer than our ACK timeout. This distinguishes waiting for an ACK
      // from a true stall.
      if (!stallActive && (nowMs - stallDetectStartMs) > (LORA_COMM_ACK_TIMEOUT_MS + 200)) { // 200ms buffer
        stallActive = true;
        Logger::printf(Logger::Level::Warn, "lora", "stall detected: obx=%u, rs=%d", outboxCount, (int)radioState);
      }
    } else {
      // Outbox is empty or we are transmitting. Reset stall detection.
      if (stallActive) {
        stallActive = false;
        Logger::printf(Logger::Level::Info, "lora", "stall cleared");
      }
      stallDetectStartMs = 0; // Reset timer
    }
    // Cleanup: drop expired messages
    compactOutbox();
    // Low-noise periodic stats (Verbose): every 5s
    LOG_EVERY_MS(5000, {
      Logger::printf(Logger::Level::Verbose, "lora", "stats tx=%u rx_data=%u rx_ack=%u drop=%u obx_max=%u", (unsigned)statsTx, (unsigned)statsRxData, (unsigned)statsRxAck, (unsigned)statsDropped, (unsigned)statsOutboxMax);
      statsTx = statsRxData = statsRxAck = statsDropped = 0;
      statsOutboxMax = outboxCount;
    });
  }

  bool isConnected() const {
    if (mode == Mode::Master) return connectionState == ConnectionState::Connected;
    return connectionState == ConnectionState::Connected;
  }
  int16_t getLastRssiDbm() const {
    // Return invalid RSSI if no packets have been received yet
    if (lastAckOkMs == 0) {
      return INT16_MIN;  // Special value indicating no valid RSSI
    }
    return lastRssiDbm;
  }
  size_t getPeerCount() const {
    // Only count active/connected peers (this is what users expect to see)
    size_t c = 0;
    for (size_t i = 0; i < LORA_COMM_MAX_PEERS; i++) if (peers[i].peerId != 0 && peers[i].connected) c++;
    return c;
  }

  size_t getTotalPeerCount() const {
    // Return total number of peers (active + inactive) - for debugging/internal use
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

  void forceReconnect() {
    if (mode == Mode::Slave) {
        connectionState = ConnectionState::Connecting;
        nextReconnectAttemptMs = millis() + LORA_COMM_RECONNECT_ATTEMPT_MS;
    }
    // For Master, there's no "reconnect" action to take. It just listens.
    // We can log this event for debugging if needed.
    else {
        Logger::printf(Logger::Level::Debug, "lora", "forceReconnect called on Master - no action taken.");
    }
  }

  void updateConnectionState(uint32_t nowMs) {
    // Connection state management for both Master and Slave
    if (mode == Mode::Master) {
      // Master is connected if it has at least one active peer
      bool hasActivePeers = false;
      for (size_t i = 0; i < LORA_COMM_MAX_PEERS; i++) {
        if (peers[i].peerId != 0 && peers[i].connected) {
          hasActivePeers = true;
          break;
        }
      }
      connectionState = hasActivePeers ? ConnectionState::Connected : ConnectionState::Disconnected;
    } else {
      // Slave connection logic - more robust state management
      if (mode == Mode::Slave) {
        bool hasRecentActivity = false;

        // Check for recent ACKs or any frame from the master
        if (lastAckOkMs != 0 && ((int32_t)(nowMs - lastAckOkMs) < (int32_t)peerTimeoutMs)) {
            hasRecentActivity = true;
        } else {
            for (size_t i = 0; i < LORA_COMM_MAX_PEERS; i++) {
                if (peers[i].peerId == masterNodeId && peers[i].connected) {
                    hasRecentActivity = true;
                    break;
                }
            }
        }

        if (hasRecentActivity) {
            if (connectionState != ConnectionState::Connected) {
                connectionState = ConnectionState::Connected;
                Logger::printf(Logger::Level::Info, "lora", "Connection established with master %u", masterNodeId);
            }
            return; // Healthy state, early exit.
        }

        // If we get here, there is no recent activity from the master.
        switch (connectionState) {
            case ConnectionState::Connected:
                // We were connected, but have now lost contact.
                connectionState = ConnectionState::Disconnected;
                nextReconnectAttemptMs = nowMs; // Trigger immediate reconnect attempt
                Logger::printf(Logger::Level::Info, "lora", "Connection lost, will attempt reconnect");
                break;

            case ConnectionState::Disconnected:
                if (nowMs >= nextReconnectAttemptMs) {
                    connectionState = ConnectionState::Connecting;
                    connectionAttemptStartMs = nowMs;
                    if (sendData(masterNodeId, nullptr, 0, true)) {
                        Logger::printf(Logger::Level::Info, "lora", "Sent reconnection frame to master %u", masterNodeId);
                    } else {
                        // Outbox was full, couldn't send. Revert to disconnected and try again shortly.
                        Logger::printf(Logger::Level::Warn, "lora", "Outbox full, delaying reconnect attempt.");
                        connectionState = ConnectionState::Disconnected;
                        nextReconnectAttemptMs = nowMs + 500; // Retry quickly
                    }
                }
                break;

            case ConnectionState::Connecting:
                // Timeout for the connecting state. If we don't get an ACK or any frame
                // within the total retry period, assume failure and go back to disconnected.
                const uint32_t connectingTimeout = (LORA_COMM_ACK_TIMEOUT_MS * LORA_COMM_MAX_RETRIES) + 2000; // Total retry time + 2s buffer
                if (nowMs - connectionAttemptStartMs > connectingTimeout) {
                    Logger::printf(Logger::Level::Warn, "lora", "Connection attempt timed out.");
                    connectionState = ConnectionState::Disconnected;
                    nextReconnectAttemptMs = nowMs + LORA_COMM_RECONNECT_ATTEMPT_MS; // Schedule next full attempt
                }
                break;
        }
      }
    }
  }

private:
  // Internal unsafe begin - should not be called directly
  void unsafeBegin(Mode m, uint8_t id) {
    mode = m;
    selfId = id;

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

    initialized = true;
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
  uint32_t lastAckOkMs;
  uint32_t lastRadioActivityMs;
  uint32_t lastNowMs;
  int16_t lastRssiDbm;
  uint32_t peerTimeoutMs = 15000; // Default, can be overridden

  volatile bool rxPendingAck;
  volatile uint8_t rxAckTargetId;
  volatile uint16_t rxAckMessageId;

  // Connection management
  uint32_t lastConnectionCheckMs;
  uint32_t nextReconnectAttemptMs;
  uint8_t masterNodeId;
  ConnectionState connectionState;
  uint32_t connectionAttemptStartMs = 0;

  OutMsg outbox[LORA_COMM_MAX_OUTBOX];
  uint8_t outboxCount;
  uint16_t nextMessageId;
  uint16_t awaitingAckMsgId;
  uint8_t awaitingAckSrcId;
  uint16_t currentTxMsgId = 0;
  uint8_t txStuckConsecutive = 0;

  PeerInfo peers[LORA_COMM_MAX_PEERS];

  RadioEvents_t radioEvents;
  State radioState;
  bool initialized;

  bool verboseEnabled = false;
  uint8_t logLevel = (uint8_t)Logger::Level::Info;
  bool stallActive;
  uint32_t stallDetectStartMs = 0; // For improved stall detection

  // Aggregated stats for low-noise periodic reporting
  uint16_t statsRxData = 0;
  uint16_t statsRxAck = 0;
  uint16_t statsTx = 0;
  uint16_t statsTxTimeouts = 0;
  uint16_t statsDropped = 0;
  uint8_t statsOutboxMax = 0;

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
    const uint16_t justSentMsgId = currentTxMsgId;
    txStuckConsecutive = 0;
    // If we just sent a non-ACK message (e.g., PING or best-effort DATA),
    // drop it from the outbox immediately to avoid backlog growth.
    if (justSentMsgId != 0) {
      for (size_t i = 0; i < outboxCount; i++) {
        OutMsg &m = outbox[i];
        if (m.inUse && m.msgId == justSentMsgId) {
          if (!m.requireAck) {
            m.inUse = false;
            compactOutbox();
          }
          break;
        }
      }
    }
    currentTxMsgId = 0;
    enterRxMode();
  }

  void onTxTimeout() {
    lastRadioActivityMs = millis();
    radioState = State::Idle;
    Logger::printf(Logger::Level::Warn, "lora", "TX timeout");
    statsTxTimeouts++;
    const uint16_t timedOutMsgId = currentTxMsgId;
    txStuckConsecutive = 0;
    if (timedOutMsgId != 0) {
      for (size_t i = 0; i < outboxCount; i++) {
        OutMsg &m = outbox[i];
        if (m.inUse && m.msgId == timedOutMsgId) {
          if (m.requireAck) {
            m.nextAttemptMs = millis() + LORA_COMM_ACK_TIMEOUT_MS;
          } else {
            m.inUse = false;
            statsDropped++;
            Logger::printf(Logger::Level::Warn, "lora", "drop (timeout) msgId=%u", m.msgId);
          }
          break;
        }
      }
      compactOutbox();
    }
    currentTxMsgId = 0;
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

    // Track peers and basic presence (any valid frame counts)
    // This is important for both master and slave connection state tracking
    notePeerSeen(src, millis());

    switch (type) {
      case FrameType::Ack: {
        // ACK for our outgoing message
        if (onAckCb != nullptr) onAckCb(src, msgId);
        statsRxAck++;
        Logger::printf(Logger::Level::Debug, "lora", "RX ACK from=%u msgId=%u", src, msgId);
        if (mode == Mode::Slave) {
          lastAckOkMs = millis();
          // Let the tick handler manage connection state transitions.
          // Setting state here creates race conditions.
        } else if (mode == Mode::Master) {
          // Master connection state is updated in updateConnectionState based on peer activity
          // notePeerSeen is already called above for all frame types
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
        statsRxData++;
        Logger::printf(Logger::Level::Info, "lora", "RX DATA from=%u len=%u", src, appLen);
        if (onDataCb != nullptr && appLen > 0) {
          onDataCb(src, payload + kHeaderSize, appLen);
        }
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
    lastRadioActivityMs = millis();
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
    // Ensure clean transition out of RX before TX
    Radio.Sleep();
    delay(2);
    Radio.Standby();
    delay(3);
    lastRadioActivityMs = millis();
    Radio.Send(frame, length);
    radioState = State::Tx;
  }

  int selectNextOutboxIndex(uint32_t nowMs) {
    // 1) Prefer retries that are due
    int bestIdx = -1;
    uint32_t bestDue = 0;
    for (int i = 0; i < (int)outboxCount; i++) {
      OutMsg &m = outbox[i];
      if (!m.inUse) continue;
      if (m.requireAck && m.attempts > 0 && m.attempts < LORA_COMM_MAX_RETRIES) {
        if ((int32_t)(nowMs - m.nextAttemptMs) >= 0) {
          // Due now; choose the earliest due
          if (bestIdx < 0 || m.nextAttemptMs - nowMs < bestDue) {
            bestIdx = i;
            bestDue = (uint32_t)(m.nextAttemptMs - nowMs);
          }
        }
      }
    }
    if (bestIdx >= 0) return bestIdx;
    // 2) Then send never-attempted messages
    for (int i = 0; i < (int)outboxCount; i++) {
      OutMsg &m = outbox[i];
      if (!m.inUse) continue;
      if (m.attempts == 0) return i;
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
        // drop (count, log once per drop at Warn)
        statsDropped++;
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
        // Let the tick handler manage the 'connected' state based on timeout.
        return;
      }
    }
    // Insert into empty slot
    for (size_t i = 0; i < LORA_COMM_MAX_PEERS; i++) {
      if (peers[i].peerId == 0) {
        peers[i].peerId = peerId;
        peers[i].lastSeenMs = nowMs;
        // The connected flag will be set by the tick handler on its next run.
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
    // The connected flag will be set by the tick handler on its next run.
  }

  void reinitializeRadio() {
    Radio.Sleep();
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
    delay(5);
    enterRxMode();
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
  lora.begin(LoRaComm::Mode::Slave, 3);
  lora.setOnDataReceived(onData);
  lora.setOnAckReceived(onAck);
}

void loop() {
  lora.tick(millis());
  delay(1);
}
#endif
