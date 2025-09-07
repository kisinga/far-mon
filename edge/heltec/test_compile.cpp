// Test compilation file to check for basic include issues
#include "lib/common_app.h"
#include "lib/device_config.h"
#include "lib/system_services.h"
#include "lib/task_manager.h"
#include "lib/display_provider.h"
#include "lib/wifi_manager.h"
#include "lib/wifi_config.h"

int main() {
    // Test basic instantiation
    auto config = createRelayConfig("01");
    WifiManager wifiManager(wifiConfig);

    return 0;
}
