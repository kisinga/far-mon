#include "hal_lorawan.h"
#include "LoRaWan_APP.h"
#include "core_logger.h"

// Static instance for C callbacks
LoRaWANHal* LoRaWANHal::instance = nullptr;

LoRaWANHal::LoRaWANHal() {
    instance = this;
}

bool LoRaWANHal::begin(const uint8_t* devEui, const uint8_t* appEui, const uint8_t* appKey) {
    if (initialized) {
        LOGW("LoRaWAN", "Already initialized");
        return false;
    }

    // Copy keys to internal buffers
    memcpy(loraWanKeys.appEui, appEui, 8);
    memcpy(loraWanKeys.devEui, devEui, 8);
    memcpy(loraWanKeys.appKey, appKey, 16);

    // Initialize LoRaWAN stack
    loraWanStatus.UserTempBufferSize = 0;
    loraWanStatus.UserTempBuffer = nullptr;

    // Set default configuration
    loraWanStatus.DevClass = CLASS_A;
    loraWanStatus.MType = UNCONFIRMED_DATA_UP;
    loraWanStatus.Channel = 0;

    // Initialize radio
    Radio.Init(&RadioEvents);

    // Initialize LoRaWAN
    loraWanInit();

    // Set callbacks
    RadioEvents.TxDone = onLoRaWANTxDone;
    RadioEvents.TxTimeout = onLoRaWANTxTimeout;
    RadioEvents.RxDone = onLoRaWANRxDone;

    initialized = true;
    LOGI("LoRaWAN", "HAL initialized");

    return true;
}

void LoRaWANHal::tick(uint32_t nowMs) {
    if (!initialized) return;

    // Update connection state based on activity
    if (lastActivityMs > 0 && (nowMs - lastActivityMs) > 60000) { // 1 minute timeout
        if (connectionState == ConnectionState::Connected) {
            connectionState = ConnectionState::Disconnected;
            LOGI("LoRaWAN", "Connection timeout");
        }
    }
}

bool LoRaWANHal::sendData(uint8_t port, const uint8_t *payload, uint8_t length, bool confirmed) {
    if (!initialized || !isJoined()) {
        LOGW("LoRaWAN", "Not initialized or not joined");
        return false;
    }

    if (length > 242) { // LoRaWAN max payload size for most regions
        LOGW("LoRaWAN", "Payload too large: %d bytes", length);
        return false;
    }

    // Prepare application data
    lora_AppData_t appData;
    appData.Port = port;
    appData.Buff = const_cast<uint8_t*>(payload);
    appData.BuffSize = length;
    appData.BuffPtr = appData.Buff;

    // Set message type
    loraWanStatus.MType = confirmed ? CONFIRMED_DATA_UP : UNCONFIRMED_DATA_UP;

    // Send data
    loraWanSend(&appData);

    uplinkCount++;
    LOGD("LoRaWAN", "Sent %d bytes on port %d (confirmed: %s)",
         length, port, confirmed ? "true" : "false");

    return true;
}

bool LoRaWANHal::isReadyForTx() const {
    if (!initialized) return false;
    // In LoRaWAN, we can always queue messages, but we should check duty cycle
    return isJoined();
}

void LoRaWANHal::setOnDataReceived(OnDataReceived cb) {
    onDataCb = cb;
}

void LoRaWANHal::setOnTxDone(OnTxDone cb) {
    onTxDoneCb = cb;
}

void LoRaWANHal::setOnTxTimeout(OnTxTimeout cb) {
    onTxTimeoutCb = cb;
}

bool LoRaWANHal::isConnected() const {
    return connectionState == ConnectionState::Connected;
}

ILoRaWANHal::ConnectionState LoRaWANHal::getConnectionState() const {
    return connectionState;
}

int16_t LoRaWANHal::getLastRssiDbm() const {
    return lastRssiDbm;
}

int8_t LoRaWANHal::getLastSnr() const {
    return lastSnr;
}

void LoRaWANHal::setDeviceClass(uint8_t deviceClass) {
    if (initialized) {
        loraWanStatus.DevClass = (DeviceClass_t)deviceClass;
        LOGI("LoRaWAN", "Device class set to %d", deviceClass);
    }
}

void LoRaWANHal::setDataRate(uint8_t dataRate) {
    if (initialized) {
        // This would require more complex LoRaWAN stack integration
        LOGD("LoRaWAN", "Data rate setting not implemented in this version");
    }
}

void LoRaWANHal::setTxPower(uint8_t txPower) {
    if (initialized) {
        // This would require more complex LoRaWAN stack integration
        LOGD("LoRaWAN", "TX power setting not implemented in this version");
    }
}

void LoRaWANHal::setAdr(bool enable) {
    if (initialized) {
        loraWanStatus.ADR = enable;
        LOGI("LoRaWAN", "ADR %s", enable ? "enabled" : "disabled");
    }
}

bool LoRaWANHal::isJoined() const {
    return initialized && (loraWanStatus.Status == LORAWAN_JOINED);
}

void LoRaWANHal::join() {
    if (!initialized) {
        LOGE("LoRaWAN", "Not initialized");
        return;
    }

    LOGI("LoRaWAN", "Starting join process");
    connectionState = ConnectionState::Connecting;
    loraWanJoin();
}

void LoRaWANHal::forceReconnect() {
    if (!initialized) return;

    LOGI("LoRaWAN", "Forcing reconnect");
    connectionState = ConnectionState::Disconnected;
    join();
}

uint32_t LoRaWANHal::getUplinkCount() const {
    return uplinkCount;
}

uint32_t LoRaWANHal::getDownlinkCount() const {
    return downlinkCount;
}

void LoRaWANHal::resetCounters() {
    uplinkCount = 0;
    downlinkCount = 0;
    LOGI("LoRaWAN", "Counters reset");
}

// Static callback functions for LoRaWAN_APP
void LoRaWANHal::onLoRaWANRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
    if (instance) {
        instance->handleRxDone(payload, size, rssi, snr);
    }
}

void LoRaWANHal::onLoRaWANTxDone(void) {
    if (instance) {
        instance->handleTxDone();
    }
}

void LoRaWANHal::onLoRaWANTxTimeout(void) {
    if (instance) {
        instance->handleTxTimeout();
    }
}

void LoRaWANHal::onLoRaWANJoinDone(void) {
    if (instance) {
        instance->handleJoinDone();
    }
}

// Instance callback handlers
void LoRaWANHal::handleRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
    lastActivityMs = millis();
    lastRssiDbm = rssi;
    lastSnr = snr;
    downlinkCount++;

    if (connectionState != ConnectionState::Connected) {
        connectionState = ConnectionState::Connected;
        LOGI("LoRaWAN", "Connected to network");
    }

    LOGD("LoRaWAN", "Received %d bytes, RSSI: %d dBm, SNR: %d dB",
         size, rssi, snr);

    if (onDataCb && size > 0) {
        // Extract port from LoRaWAN frame
        uint8_t port = 1; // Default port
        if (size > 0) {
            // In a real implementation, we'd parse the LoRaWAN frame header
            // For now, assume port 1
            onDataCb(port, payload, size);
        }
    }
}

void LoRaWANHal::handleTxDone(void) {
    lastActivityMs = millis();

    LOGD("LoRaWAN", "TX completed");

    if (onTxDoneCb) {
        onTxDoneCb();
    }
}

void LoRaWANHal::handleTxTimeout(void) {
    LOGW("LoRaWAN", "TX timeout");

    if (onTxTimeoutCb) {
        onTxTimeoutCb();
    }
}

void LoRaWANHal::handleJoinDone(void) {
    if (loraWanStatus.Status == LORAWAN_JOINED) {
        connectionState = ConnectionState::Connected;
        LOGI("LoRaWAN", "Successfully joined network");
    } else {
        connectionState = ConnectionState::Disconnected;
        LOGW("LoRaWAN", "Join failed");
    }
}
