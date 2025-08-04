/*
  Heltec Relay LoRaWAN Sketch
  
  Goal:
  - Act as a LoRaWAN end-device, based on the working example.
  - Support a simulation/debug mode for testing without a real LoRaWAN network.
*/

#include "Arduino.h"
#include "LoRaWan_APP.h"

// --- LoRaWAN & App Configuration ---
/* OTAA para*/
uint8_t devEui[] = { 0x22, 0x32, 0x33, 0x00, 0x00, 0x88, 0x88, 0x02 };
uint8_t appEui[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t appKey[] = { 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x66, 0x01 };

/* ABP para*/
uint8_t nwkSKey[] = { 0x15, 0xb1, 0xd0, 0xef, 0xa4, 0x63, 0xdf, 0xbe, 0x3d, 0x11, 0x18, 0x1e, 0x1e, 0xc7, 0xda,0x85 };
uint8_t appSKey[] = { 0xd7, 0x2c, 0x78, 0x75, 0x8c, 0xdc, 0xca, 0xbf, 0x55, 0xee, 0x4a, 0x77, 0x8d, 0x16, 0xef,0x67 };
uint32_t devAddr =  ( uint32_t )0x007e6ae1;

/*LoraWan channelsmask, default channels 0-7*/ 
uint16_t userChannelsMask[6]={ 0x00FF,0x0000,0x0000,0x0000,0x0000,0x0000 };

/*LoraWan region, select in arduino IDE tools*/
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;

/*LoraWan Class, Class A and Class C are supported*/
DeviceClass_t  loraWanClass = CLASS_C;

/*the application data transmission duty cycle.  value in [ms].*/
uint32_t appTxDutyCycle = 15000;

/*OTAA or ABP*/
bool overTheAirActivation = true;

/*ADR enable*/
bool loraWanAdr = true;

/* Indicates if the node is sending confirmed or unconfirmed messages */
bool isTxConfirmed = true;

/* Application port */
uint8_t appPort = 2;

uint8_t confirmedNbTrials = 4;

// --- State Management ---
bool debugMode = true; // Set to true to simulate, false for real LoRaWAN
uint32_t uplinkCounter = 0;

static void prepareTxFrame( uint8_t port )
{
    // This is where you would read sensor data
    // For now, we'll just send an uplink counter
    uplinkCounter++;
    appDataSize = 4; //sizeof(uplinkCounter);
    appData[0] = uplinkCounter >> 24;
    appData[1] = uplinkCounter >> 16;
    appData[2] = uplinkCounter >> 8;
    appData[3] = uplinkCounter;

    Serial.println("Prepared Uplink #" + String(uplinkCounter));
}

void downLinkDataHandle(McpsIndication_t *mcpsIndication)
{
  String downlinkPayload = "";
  for(uint8_t i=0; i<mcpsIndication->BufferSize; i++) {
    downlinkPayload += (char)mcpsIndication->Buffer[i];
  }
  
  Serial.println("Downlink received: " + downlinkPayload);
}

void setup() {
    Serial.begin(115200);
    // while (!Serial); // This blocks until a serial connection is made.
    Serial.println("Booting...");

    if (debugMode) {
      Serial.println("DEBUG MODE: Send commands via Serial Monitor (e.g., 'join', 'send')");
    } else {
      // Mcu.begin() initializes the board hardware.
      Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
    }
}

void runLoRaWAN() {
  // This is the standard LoRaWAN state machine from the example
  switch( deviceState )
  {
    case DEVICE_STATE_INIT:
    {
      #if(LORAWAN_DEVEUI_AUTO)
        LoRaWAN.generateDeveuiByChipID();
      #endif
      LoRaWAN.init(loraWanClass,loraWanRegion);
      LoRaWAN.setDefaultDR(3);
      deviceState = DEVICE_STATE_JOIN;
      break;
    }
    case DEVICE_STATE_JOIN:
    {
      Serial.println("Joining...");
      LoRaWAN.join();
      break;
    }
    case DEVICE_STATE_SEND:
    {
      prepareTxFrame( appPort );
      LoRaWAN.send();
      deviceState = DEVICE_STATE_CYCLE;
      break;
    }
    case DEVICE_STATE_CYCLE:
    {
      // Schedule next packet transmission
      txDutyCycleTime = appTxDutyCycle + randr( -APP_TX_DUTYCYCLE_RND, APP_TX_DUTYCYCLE_RND );
      LoRaWAN.cycle(txDutyCycleTime);
      deviceState = DEVICE_STATE_SLEEP;
      break;
    }
    case DEVICE_STATE_SLEEP:
    {
      LoRaWAN.sleep(loraWanClass);
      break;
    }
    default:
    {
      deviceState = DEVICE_STATE_INIT;
      break;
    }
  }
}

void runDebugMode() {
  // In debug mode, we simulate the LoRaWAN state machine via Serial commands
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "join") {
      Serial.println("Simulating Join...");
      delay(1000);
      Serial.println("Join Success (Simulated)");
    } else if (cmd == "send") {
      Serial.println("Simulating Send...");
      prepareTxFrame(appPort);
      Serial.println("Uplink Sent (Simulated). Payload: " + String(uplinkCounter));
    } else if (cmd.startsWith("downlink ")) {
      String payload = cmd.substring(9);
      Serial.println("Simulated Downlink: " + payload);
    } else {
      Serial.println("Unknown command: " + cmd);
    }
  }
}

void loop() {
  if (debugMode) {
    runDebugMode();
  } else {
    runLoRaWAN();
  }
}
