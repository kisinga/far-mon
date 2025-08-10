// Remote Node Firmware for Heltec WiFi LoRa 32 (V3)
// ---------------------------------------------------
// This sketch demonstrates a modular firmware architecture based on the
// following components:
//   * DisplayManager (OLED UI)
//   * StateManager   (Central device state)
//   * CommsManager   (LoRa + Serial comms)
//   * AsyncTaskManager (FreeRTOS periodic tasks)
//
// Hardware: Heltec WiFi LoRa 32 V3 (ESP32-C3) – 863-928 MHz band
// NOTE: Designed for the Arduino IDE – **no PlatformIO**
// ----------------------------------------------------

#include <Arduino.h>
#include "../lib/display.h"
#include "../lib/State.h"
#include "../lib/Comms.h"
#include "../lib/AsyncTask.h"
#include "../lib/images.h"   // optional – contains boot logo bitmap (can be empty)


// -----------------------------------------------------------------------------
// LoRa configuration (adjust for your region – 915 MHz used by default)
// -----------------------------------------------------------------------------
#define LORA_FREQUENCY 915000000 // 915 MHz (US)
#define SERIAL_BAUD    115200

// Forward declaration of helper callbacks
void onLoRaMessage(const LoRaMessage &msg);
void sendSensorDataTask();

// -----------------------------------------------------------------------------
// Instantiate our managers – dependency injection keeps them decoupled.
// -----------------------------------------------------------------------------
DisplayManager   screen;
StateManager     state(&screen);
CommsManager     comms(LORA_FREQUENCY, onLoRaMessage);
AsyncTaskManager asyncMgr;

// Example sensor pins (dummy)
const int TEMP_SENSOR_PIN = 34;
const int HUM_SENSOR_PIN  = 35;

// Cached string with last uplink time (for display)
String lastUplink = "---";

void setup() {
    // Serial first for early debug output
    Serial.begin(SERIAL_BAUD);
    delay(1000);
    Serial.println("\n[Remote] Booting...");

    // 1. Display boot logo
    screen.init();
    screen.showBootLogo();

    // 2. Initialise State machine
    state.init();

    // 3. Initialise Communications – LoRa + Serial echo
    comms.initSerial(SERIAL_BAUD);
    comms.initLoRa();

    // 4. Register periodic tasks (every 10 s send sensor data)
    asyncMgr.init();
    asyncMgr.registerTask("uplink", 10000UL, sendSensorDataTask);

    Serial.println("[Remote] Setup complete. Entering main loop…");
}

void loop() {
    // Let communications process interrupts / queues
    comms.loop();

    // Async tasks are executed in their own FreeRTOS context, but loop() gives
    // us an opportunity for any lightweight main-thread processes if needed.

    // Small delay to avoid watchdog triggers / busy-looping
    delay(5);
}

// -----------------------------------------------------------------------------
// Callbacks / Tasks
// -----------------------------------------------------------------------------

// Called whenever the CommsManager receives a LoRa frame.
void onLoRaMessage(const LoRaMessage &msg) {
    Serial.printf("[Remote] RX: %s (RSSI %d)\n", msg.incomingMessage.c_str(), msg.rssi);

    // Simple command parser – expects "interval=<seconds>" to change uplink
    if (msg.incomingMessage.startsWith("interval=")) {
        unsigned long newInterval = msg.incomingMessage.substring(9).toInt() * 1000UL;
        if (newInterval >= 1000UL) {
            Serial.printf("[Remote] Updating interval to %lu ms\n", newInterval);
            // Re-register task with new interval
            asyncMgr.registerTask("uplink", newInterval, sendSensorDataTask);
        }
    }
}

// Periodic task – read sensors & send uplink via LoRa
void sendSensorDataTask() {
    // Dummy sensor readings – replace with real sensors
    float temperature = analogRead(TEMP_SENSOR_PIN) / 4095.0 * 100.0; // fake 0-100°C
    float humidity    = analogRead(HUM_SENSOR_PIN) / 4095.0 * 100.0; // fake 0-100%

    char payload[64];
    snprintf(payload, sizeof(payload), "id=%s,temp=%.1f,hum=%.1f", state.getDeviceId(), temperature, humidity);

    Serial.printf("[Remote] TX: %s\n", payload);
    state.updateState(SENDING);
    comms.sendLoRaMessage(String(payload), 0); // broadcast

    // Update display
    lastUplink = String(millis() / 1000);
    screen.updateDisplay(state.getDeviceId(), "SENDING", lastUplink.c_str());

    // Switch state back to idle after a short delay
    state.updateState(IDLE);
}

