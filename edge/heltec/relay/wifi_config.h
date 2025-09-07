#pragma once

// WiFi Configuration for Relay device - DRY Implementation
// Now uses the unified WiFi configuration system

#include "../lib/wifi_config.h"

// Relay-specific WiFi configuration
static WifiManager::Config wifiConfig = WiFiConfigFactory::createRelayConfig();

// For backward compatibility, define the old constants
static const char WIFI_SSID[] = RELAY_WIFI_SSID;
static const char WIFI_PASSWORD[] = RELAY_WIFI_PASSWORD;


