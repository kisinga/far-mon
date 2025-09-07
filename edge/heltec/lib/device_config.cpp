// Device Configuration Implementation

#include "device_config.h"

// Factory function for relay configuration
RelayConfig createRelayConfig(const char* deviceId) {
    RelayConfig config;
    config.deviceId = deviceId;
    return config;
}

// Factory function for remote configuration
RemoteConfig createRemoteConfig(const char* deviceId) {
    RemoteConfig config;
    config.deviceId = deviceId;
    return config;
}
