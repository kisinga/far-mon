#include "remote_config.h"

RemoteConfig createRemoteConfig(const char* deviceId) {
    RemoteConfig config;
    config.deviceId = deviceId;
    return config;
}


