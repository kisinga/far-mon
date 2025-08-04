/*
  Heltec Relay LoRaWAN Sketch
*/

#include "Arduino.h"
#include "LoRaWan_APP.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "logo.h"

static SSD1306Wire  display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// --- LoRaWAN & App Configuration ---
uint8_t devEui[] = { 0x22, 0x32, 0x33, 0x00, 0x00, 0x88, 0x88, 0x02 };
uint8_t appEui[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t appKey[] = { 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x66, 0x01 };
uint8_t nwkSKey[] = { 0x15, 0xb1, 0xd0, 0xef, 0xa4, 0x63, 0xdf, 0xbe, 0x3d, 0x11, 0x18, 0x1e, 0x1e, 0xc7, 0xda,0x85 };
uint8_t appSKey[] = { 0xd7, 0x2c, 0x78, 0x75, 0x8c, 0xdc, 0xca, 0xbf, 0x55, 0xee, 0x4a, 0x77, 0x8d, 0x16, 0xef,0x67 };
uint32_t devAddr =  ( uint32_t )0x007e6ae1;
uint16_t userChannelsMask[6]={ 0x00FF,0x0000,0x0000,0x0000,0x0000,0x0000 };
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;
DeviceClass_t  loraWanClass = CLASS_C;
uint32_t appTxDutyCycle = 15000;
bool overTheAirActivation = true;
bool loraWanAdr = true;
bool isTxConfirmed = true;
uint8_t appPort = 2;
uint8_t confirmedNbTrials = 4;

// --- State Management ---
bool debugMode = true;
uint32_t uplinkCounter = 0;

// --- Display State ---
int lastRssi = 0;
int lastSnr = 0;
String temporaryMessage = "";
unsigned long temporaryMessageExpiry = 0;
unsigned long lastDisplayCheck = 0;
const int displayCheckInterval = 250; // ms, how often to check if we need to redraw

void showTemporaryMessage(String msg, int duration);
void drawStatusScreen();

static void prepareTxFrame( uint8_t port )
{
    uplinkCounter++;
    appDataSize = 4;
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
  lastRssi = mcpsIndication->Rssi;
  lastSnr = mcpsIndication->Snr;
  showTemporaryMessage("Downlink Rx", 2000);
}

void VextON(void) {
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext, LOW);
}

String getDeviceID() {
  char buff[5];
  sprintf(buff, "%02X%02X", devEui[6], devEui[7]);
  return String(buff);
}

String deviceStateToString(eDeviceState_LoraWan state) {
  switch(state) {
    case DEVICE_STATE_INIT: return "INIT";
    case DEVICE_STATE_JOIN: return "JOINING";
    case DEVICE_STATE_SEND: return "SENDING";
    case DEVICE_STATE_CYCLE: return "IDLE";
    case DEVICE_STATE_SLEEP: return "SLEEPING";
    default: return "UNKNOWN";
  }
}

void drawRssiBars(int rssi) {
  int numBars = 0;
  if (rssi > -80) numBars = 4;
  else if (rssi > -95) numBars = 3;
  else if (rssi > -110) numBars = 2;
  else if (rssi != 0) numBars = 1;

  for (int i = 0; i < 4; i++) {
    if (i < numBars) {
      display.fillRect(120 - (i * 4), 60 - (i * 2), 3, 2 + (i * 2));
    } else {
      display.drawRect(120 - (i * 4), 60 - (i * 2), 3, 2 + (i * 2));
    }
  }
}

void showBootScreen() {
  display.clear();
  display.drawXbm(0, 8, farm_logo_width, farm_logo_height, farm_logo_bits);
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(55, 20, "FARM\nERP");
  display.display();
  delay(2500);
}

void drawStatusScreen() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  
  String topLine = "ID:" + getDeviceID();
  if (debugMode) {
    topLine += " [D]";
  }
  display.drawString(0, 0, topLine);

  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 18, deviceStateToString(deviceState));

  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 52, "Up:" + String(uplinkCounter) + " | SNR:" + String(lastSnr));
  
  drawRssiBars(lastRssi);
  
  display.display();
}

void showTemporaryMessage(String msg, int duration) {
  temporaryMessage = msg;
  temporaryMessageExpiry = millis() + duration;
  
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 22, msg);
  display.display();
}

void handleDisplay() {
  if (millis() < lastDisplayCheck + displayCheckInterval) {
    return;
  }
  lastDisplayCheck = millis();

  if (temporaryMessage != "" && millis() >= temporaryMessageExpiry) {
    temporaryMessage = "";
    drawStatusScreen();
  } else if (temporaryMessage == "") {
    drawStatusScreen();
  }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Booting...");

    VextON();
    delay(100);
    display.init();
    
    showBootScreen();

    if (debugMode) {
      Serial.println("DEBUG MODE: Send commands via Serial Monitor.");
    } else {
      Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
    }
}

void runLoRaWAN() {
  switch( deviceState )
  {
    case DEVICE_STATE_INIT:
      LoRaWAN.init(loraWanClass,loraWanRegion);
      deviceState = DEVICE_STATE_JOIN;
      break;
    case DEVICE_STATE_JOIN:
      LoRaWAN.join();
      break;
    case DEVICE_STATE_SEND:
      prepareTxFrame(appPort);
      LoRaWAN.send();
      showTemporaryMessage("Uplink Sent", 2000);
      deviceState = DEVICE_STATE_CYCLE;
      break;
    case DEVICE_STATE_CYCLE:
      txDutyCycleTime = appTxDutyCycle + randr( -APP_TX_DUTYCYCLE_RND, APP_TX_DUTYCYCLE_RND );
      LoRaWAN.cycle(txDutyCycleTime);
      deviceState = DEVICE_STATE_SLEEP;
      break;
    case DEVICE_STATE_SLEEP:
      LoRaWAN.sleep(loraWanClass);
      break;
    default:
      deviceState = DEVICE_STATE_INIT;
      break;
  }
}

void runDebugMode() {
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "join") {
      deviceState = DEVICE_STATE_JOIN;
      showTemporaryMessage("Joining...", 1000);
      deviceState = DEVICE_STATE_SEND;
    } else if (cmd == "send") {
      deviceState = DEVICE_STATE_SEND;
      prepareTxFrame(appPort);
      showTemporaryMessage("Uplink Sent", 2000);
      deviceState = DEVICE_STATE_SLEEP;
    } else if (cmd.startsWith("downlink ")) {
      String payload = cmd.substring(9);
      lastRssi = -50; // mock
      lastSnr = 9;    // mock
      showTemporaryMessage("Downlink Rx", 2000);
    } else {
      showTemporaryMessage("Unknown Cmd", 2000);
    }
  }
}

void loop() {
  if (debugMode) {
    runDebugMode();
  } else {
    runLoRaWAN();
  }
  
  handleDisplay();
  delay(10);
}
