#pragma once

// Heltec WiFi LoRa 32 V3 pin definitions
#define BATTERY_ADC_PIN 1
#define VBAT_CTRL 37
// Optional charger status input from STAT pin (open-drain on many charger ICs)
// If your board does not route STAT to the MCU, set to -1 to disable.
#define CHARGE_STATUS_PIN -1
// Polarity: typical charger STAT pins are active-low (LOW = charging)
#define CHARGE_STATUS_ACTIVE_LOW 1
