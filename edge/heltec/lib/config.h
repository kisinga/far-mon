// Unified configuration header
// Centralized include for all device and communication configuration types

#pragma once

// Hardware/board-level constants
#include "board_config.h"

// Communication-related configuration (USB, LoRa, WiFi, Screen, MQTT, Routing)
#include "communication_config.h"

// Device-level configuration and per-device factories (Relay/Remote)
#include "device_config.h"

// Note:
// - Prefer including this single header in application and transport code.
// - Legacy headers like relay_config.h, remote_config.h, and wifi_config.h
//   are deprecated and removed in favor of this unified entrypoint.


