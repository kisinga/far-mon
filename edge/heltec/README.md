# Heltec Firmware

This directory contains the firmware for the Heltec LoRaWAN nodes.

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