// Remote Application Implementation
// Contains only callback implementations that need to be in a separate file

// Disabled legacy implementation to avoid conflicts with simplified `remote.ino` implementation
#if 0
#include "remote_app.h"

// Static callback handlers
static RemoteApplication* remoteInstance = nullptr;

// Static variables to store callback data
static uint8_t lastAckSrc = 0;

// Static function for display callback (cannot use lambda with function pointer)
static void renderAckDisplay(SSD1306Wire& d, void* ctx) {
    d.setTextAlignment(TEXT_ALIGN_LEFT);
    d.drawString(0, 0, F("ACK"));
    d.drawString(0, 14, String("from ") + String(lastAckSrc));
}

void RemoteApplication::onLoraData(uint8_t src, const uint8_t* payload, uint8_t len) {
    (void)src;
    (void)payload;
    (void)len;
    // Keep minimal; data ACK is auto-handled
}

void RemoteApplication::onLoraAck(uint8_t src, uint16_t msgId) {
    (void)msgId;
    lastAckSrc = src;

    if (remoteInstance) {
        auto& services = remoteInstance->getServices();
        if (services.debugRouter) {
            services.debugRouter->debugFor(
                renderAckDisplay,
                nullptr,
                nullptr,
                nullptr,
                600
            );
        }
    }
}

RemoteApplication::RemoteApplication() {
    remoteInstance = this;
}
#endif
