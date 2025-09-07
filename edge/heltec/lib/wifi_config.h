// WiFi Configuration - Centralized WiFi settings for easy management
// Modify this file to configure your WiFi credentials and settings

#pragma once

#include "wifi_manager.h"

// ===== WiFi Network Configuration =====
// Replace these with your actual WiFi network details

static const char WIFI_SSID[] = "YourWiFiNetwork";           // Your WiFi network name
static const char WIFI_PASSWORD[] = "YourWiFiPassword";     // Your WiFi password

// ===== WiFi Manager Configuration =====
// These settings control WiFi behavior

static WifiManager::Config wifiConfig = {
    .ssid = WIFI_SSID,
    .password = WIFI_PASSWORD,
    .reconnectIntervalMs = 30000,    // Try to reconnect every 30 seconds when disconnected
    .statusCheckIntervalMs = 5000    // Check WiFi status every 5 seconds
};

// ===== Usage Instructions =====
/*
To configure WiFi for your relay:

1. Open this file (wifi_config.h)
2. Replace "YourWiFiNetwork" with your actual WiFi network name (SSID)
3. Replace "YourWiFiPassword" with your actual WiFi password
4. Save the file
5. Recompile and upload the relay firmware

The relay will automatically:
- Connect to your WiFi network on startup
- Monitor connection status
- Automatically reconnect if disconnected
- Display WiFi signal strength on the OLED screen
- Show connection status in serial logs

If you don't configure WiFi credentials, the relay will still work but WiFi status will show as disconnected.
*/
