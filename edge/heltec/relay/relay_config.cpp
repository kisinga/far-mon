#include "relay_config.h"

RelayConfig createRelayConfig(const char* deviceId) {
    RelayConfig config;
    config.deviceId = deviceId;
    return config;
}


