#ifndef COMMS_MANAGER_H
#define COMMS_MANAGER_H

#include <deque>
#include "LoRaWan_APP.h"

// Message structure for the queue
struct LoRaMessage {
    int  packetSize;
    int  sender;
    byte incomingMsgId;
    String  incomingMessage;
    int  rssi;
    int8_t snr; // Add SNR to message struct
};

typedef enum
{
    LOWPOWER,
    STATE_RX,
    STATE_TX
}States_t;

class CommsManager {
public:
    // using MessageCallback = std::function<void(const LoRaMessage&)>;
    typedef void (*MessageCallback)(const LoRaMessage&);

    CommsManager(long frequency, MessageCallback callback);
    static CommsManager* getInstance(); // Static method to get the instance

    void initLoRa();
    void initSerial(long baudRate);
    void sendLoRaMessage(const String& message, int recipientAddress);
    void loop();

private:
    long _frequency;
    MessageCallback _messageCallback;
    std::deque<LoRaMessage> _messageQueue;
    RadioEvents_t RadioEvents;
    States_t _loraState;

    static void onTxDoneStatic(); // Static wrappers for RadioEvents
    static void onTxTimeoutStatic();
    static void onRxDoneStatic(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

    void onTxDone(); // Non-static implementations
    void onTxTimeout();
    void onRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
    
    void parseSerialInput();
    void processMessageQueue();
    void sendMessage(const String& message, int recipientAddress);
};

#endif // COMMS_MANAGER_H