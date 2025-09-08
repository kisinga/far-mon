#pragma once

#include <Arduino.h>
#include "board_config.h"
#include "display.h"
#include "logger.h"
#include "lora_comm.h"
#include "battery_monitor.h"
#include "scheduler.h"

// A helper struct to hold references to all global objects that need initialization
struct SystemObjects {
  OledDisplay &oled;
  LoRaComm &lora;
  BatteryMonitor::BatteryMonitor &batteryMonitor;
  BatteryMonitor::Config &batteryConfig;
  // TaskScheduler<AppState, 8> &scheduler; // AppState is app-specific, will need to be generic or passed.
};

// This function will encapsulate the common initialization logic
void initializeSystem(SystemObjects &sys, const char *deviceId, bool enableOled, uint8_t selfId, void (*renderHomeCb)(SSD1306Wire &, void *), void *renderHomeCtx);
