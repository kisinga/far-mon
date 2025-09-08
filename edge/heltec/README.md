# Heltec Firmware

This directory contains the firmware for the Heltec LoRaWAN nodes.

## Getting Started (Arduino IDE)

- Prerequisites

  - Arduino IDE 2.x
  - USB data cable (CP210x driver may be required on some systems)

- Install Heltec ESP32 board support

  1. Open Arduino IDE → File → Preferences.
  2. In “Additional Boards Manager URLs”, add: `https://resource.heltec.cn/download/package_heltec_esp32_index.json`
  3. Open Boards Manager, search for "heltec esp32", install “Heltec ESP32 Dev-Boards by Heltec Automation”.
  4. Optional: In Library Manager, search “HELTEC ESP32” and install the Heltec ESP32 Series Library.

- Select the correct board and port

  1. Tools → Board → Select other board & port… → search and choose your exact model (e.g., “Heltec WiFi LoRa 32 (V3)”).
  2. Tools → Port → select the detected serial port for your device.

- Build and upload
  - Open a sketch (or one of the Heltec examples), then Verify/Upload.
  - If upload fails, enter bootloader: hold PRG (USER/BOOT), press RST, then release PRG.

Reference: [Heltec ESP32 Series Quick Start — Arduino IDE](https://docs.heltec.org/en/node/esp32/esp32_general_docs/quick_start.html#esp32-via-arduino-ide)

### Linux upload prerequisites (serial + Python)

- Python serial module required by esptool

  - If upload fails with `ModuleNotFoundError: No module named 'serial'`, install pyserial for your user:
    - `python3 -m ensurepip --user` (if pip missing)
    - `python3 -m pip install --user pyserial`
  - System-wide alternatives also work (e.g., `sudo dnf install python3-pyserial` on Fedora), but user-level is sufficient.

- Serial port not found (e.g., `Could not open /dev/ttyUSB0`)
  - Check cable/port: use a known-good data cable; try another USB port.
  - Verify device enumerates:
    - `ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null`
    - `dmesg -w` and replug; look for CP210x/CH34x messages.
  - Add user to serial group and re-login:
    - Fedora/Ubuntu: `sudo usermod -aG dialout $USER`
    - Some distros: `sudo usermod -aG uucp $USER`
    - Then log out/in (or `newgrp dialout`).
  - ModemManager may grab the port on some distros (e.g., Fedora/Ubuntu):
    - Temporarily stop it: `sudo systemctl stop ModemManager` (restart with `start` when done).
  - Select the correct port in Arduino IDE: Tools → Port.
  - Enter bootloader if needed: hold PRG (BOOT), press RST, then release PRG; start upload.
  - Flatpak Arduino IDE users may need extra device permissions; prefer the native IDE if serial devices aren’t visible.

## Architecture

The system consists of two types of nodes:

- **Remote Nodes:** Battery-powered sensor nodes that collect data (e.g., temperature, humidity) and transmit it periodically.
- **Relay Node:** A mains-powered node that receives data from remote nodes via LoRa and forwards it to a Raspberry Pi bridge via a serial connection. It can also receive commands from the bridge to be sent as LoRa downlinks to remote nodes.

This is a "LoRa-to-serial" architecture, not LoRaWAN. The devices communicate directly.

## Data & Command Structure

To ensure interoperability, all nodes must adhere to the following structures.

### Uplink (Remote -> Relay)

A simple key-value string format is used for uplinks. Each transmission is a series of key-value pairs separated by commas, ending with a newline.

**Format:** `key1=value1,key2=value2,...`

**Example:** `id=03,temp=25.5,hum=60.2`

- `id`: The unique 1-byte ID of the remote node (e.g., `03`).
- `temp`: Temperature in Celsius.
- `hum`: Relative humidity in percent.

### Downlink (Relay -> Remote)

Downlinks are used to configure remote nodes.

**Format:** `command=value`

**Example:** `interval=60`

- `interval`: Sets the uplink interval in seconds.

This standardized structure ensures that the `relay-bridge` can parse all incoming data and construct valid commands.

I need to connect these devices
https://www.pixelelectric.com/electronic-modules/miscellaneous-modules/logic-converter/ttl-to-rs485-automatic-control-module/
https://www.pixelelectric.com/products/sensors/distance-vision/ultrasonic-proximity-sensor/jsn-sr04t-waterproof-ultrasonic-sensor/
https://www.pixelelectric.com/category/water-level-sensor-float-switch/
https://www.pixelelectric.com/sensors/load-pressure-flow-vibration/water-tds-ph-flow-level-sensor/yf-g1-dn25-1-2-100l-min-water-flow-sensor/

## Troubleshooting

- Board not visible in Arduino IDE after installing Heltec core

  - Restart Arduino IDE after installing any new board core.
  - Use Tools → Board → Select other board & port…, then search "Heltec" and select the exact model (e.g., "Heltec WiFi LoRa 32 (V3)").
  - Confirm installation in Tools → Board → Boards Manager: "Heltec ESP32 Dev-Boards by Heltec Automation" should show as Installed.
  - Ensure File → Preferences → Additional Boards Manager URLs contains the Heltec index URL. If you also use the Espressif core, ensure both URLs are present (comma-separated). After adding, reopen Boards Manager to refresh the index.

- Selected the wrong board

  - Symptom: headers like `WiFi.h` not found or other compile errors.
  - Fix: select the Heltec ESP32 board that matches your device (e.g., "Heltec WiFi LoRa 32 (V3)") instead of a generic ESP32 board.

- Boards installed but still can’t select your device

  - In Arduino IDE 2.x, prefer Tools → Board → Select other board & port… and use the search box (the short dropdown may not list new cores immediately).
  - If you can see Heltec Examples but not boards: the core is installed; restart the IDE and verify the Heltec Boards Manager URL is present, then try the board search again.

- Port doesn’t show up

  - Try a known-good USB data cable and a different USB port.
  - Linux: add your user to the serial group and re-login: `sudo usermod -aG dialout $USER` (on some distros it’s `uucp`). Then unplug/replug the board.
  - Check the device appears: `ls /dev/ttyUSB*` or `dmesg -w` while reconnecting.
  - Windows/macOS may need USB-UART drivers (e.g., CP210x or CH34x) depending on your board revision.

- Arduino CLI equivalents (optional)
  - Update indexes with an Additional URL for Heltec, then install the core:
    - `arduino-cli core update-index --additional-urls <heltec-package-index-url>`
    - `arduino-cli core search heltec` (find exact core name)
    - `arduino-cli core install <vendor:core>`
  - List detected boards/ports: `arduino-cli board list`

Note: Board and core names can vary slightly across releases. Always match the exact model printed on your Heltec board (e.g., V2 vs V3).
