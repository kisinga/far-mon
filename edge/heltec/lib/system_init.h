#pragma once

#include <Arduino.h>
#include "board_config.h"
#include "display.h"
#include "debug.h"
#include "logger.h"
#include "lora_comm.h"
#include "battery_monitor.h"
#include "scheduler.h"

// A helper struct to hold references to all global objects that need initialization
struct SystemObjects {
  OledDisplay &oled;
  DebugRouter &debugRouter;
  LoRaComm &lora;
  BatteryMonitor::BatteryMonitor &batteryMonitor;
  BatteryMonitor::Config &batteryConfig;
  // TaskScheduler<AppState, 8> &scheduler; // AppState is app-specific, will need to be generic or passed.
};

// This function will encapsulate the common initialization logic
void initializeSystem(SystemObjects &sys, const char *deviceId, bool enableOled, uint8_t selfId, void (*renderHomeCb)(SSD1306Wire &, void *), void *renderHomeCtx) {
  Serial.begin(115200);
  delay(200);
  Serial.println();

  // Logger Init
  Logger::begin(true, &sys.oled, deviceId);
  Logger::setLevel(Logger::Level::Info);
  Logger::setVerbose(false);
  LOGI("boot", "System starting...");

  // Display Init (optional)
  sys.oled.begin(enableOled);
  sys.oled.setDeviceId(deviceId);
  sys.oled.setHomescreenRenderer(renderHomeCb, renderHomeCtx);
  // sys.oled.setHeaderRightMode(HeaderRightMode::SignalBars); // Default for both remote/relay

  // Debug Router Init
  sys.debugRouter.begin(true, &sys.oled, deviceId);
  // NOTE: remote.ino disables serial output for DebugRouter. This can be a parameter if needed.

  // Battery Config
  sys.batteryConfig.adcPin = BATTERY_ADC_PIN;      // VBAT_ADC
  sys.batteryConfig.ctrlPin = VBAT_CTRL;           // VBAT_CTRL (active-low enable)
  sys.batteryConfig.samples = 12;                  // More samples for stability
  sys.batteryConfig.useHeltecV3Scaling = true;     // Use empirical scaling constant
  sys.batteryConfig.setAttenuationOnFirstRead = true;
  sys.batteryConfig.voltageEmpty = 3.04f;
  sys.batteryConfig.voltageFull = 4.26f;
  #if defined(ESP32)
  // Explicitly set attenuation for VBAT_ADC if needed and not handled internally by analogReadMilliVolts
  if (sys.batteryConfig.adcPin != 0xFF && sys.batteryConfig.setAttenuationOnFirstRead && !sys.batteryConfig._attenuationApplied) {
    ::analogSetPinAttenuation(sys.batteryConfig.adcPin, ADC_11db);
  }
  #endif
  LOGI("batt", "Heltec V3 config applied (adcPin=1, ctrlPin=37, raw/238.7)");

  // Configure charge status pin
#ifdef CHARGE_STATUS_PIN
  static const int8_t kChargeStatusPin = CHARGE_STATUS_PIN;
  #ifdef CHARGE_STATUS_ACTIVE_LOW
    static const bool kChargeActiveLow = (CHARGE_STATUS_ACTIVE_LOW != 0);
  #else
    static const bool kChargeActiveLow = true; // default assumption
  #endif
  if (kChargeStatusPin >= 0) {
    sys.batteryMonitor.initChargeDetection(kChargeStatusPin, kChargeActiveLow, millis());
    LOGI("batt", "charge status pin=%d init_raw=%d charging=%s", (int)kChargeStatusPin, (int)digitalRead((uint8_t)kChargeStatusPin), sys.batteryMonitor.isCharging() ? "yes" : "no");
  } else {
    LOGW("batt", "charge status pin not configured");
  }
#else
  LOGW("batt", "CHARGE_STATUS_PIN not defined");
#endif

  // Probe I2C/display presence and print diagnostics (only if display is enabled)
  if (enableOled) {
    sys.oled.setI2cClock(500000);
    bool found = sys.oled.probeI2C(OLED_I2C_ADDR);
    LOGI("disp", "probe 0x%02X found=%s", OLED_I2C_ADDR, found ? "yes" : "no");
    if (!found) {
      LOGW("disp", "Tips: check Vext power (LOW=ON), SDA/SCL pins, and address (0x3C vs 0x3D)");
      sys.oled.i2cScan(Serial);
    }
  }

  // LoRa Init
  sys.lora.begin(selfId == 0x01 ? LoRaComm::Mode::Master : LoRaComm::Mode::Slave, selfId);
  sys.lora.setVerbose(false);
  sys.lora.setLogLevel((uint8_t)Logger::Level::Info);

  LOGI("boot", "RF=%lu Hz tx=%d dBm", (unsigned long)LORA_COMM_RF_FREQUENCY, (int)LORA_COMM_TX_POWER_DBM);
  LOGI("boot", "System initialization complete.");
}
