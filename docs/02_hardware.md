# 2. Hardware

This document details the hardware components used in the farm monitoring system. For detailed wiring diagrams, pinouts, and datasheets, refer to the `assets` sub-directory.

## 2.1. Core Components

| Component      | Model                  | Role                                     |
| :------------- | :--------------------- | :--------------------------------------- |
| Edge Server    | Raspberry Pi 3 or 4    | Hosts ThingsBoard and the relay bridge.  |
| Remote Node    | Heltec WiFi LoRa 32 (V3) | Collects sensor data in the field.       |
| Relay Node     | Heltec WiFi LoRa 32 (V3) | Relays data between remote nodes and the Pi. |

## 2.2. Sensor Details

The system supports a variety of sensors, which are configured in the remote node's firmware.

*   **Sunverter 4b**: Connected via UART/RS485 for monitoring solar power systems.
*   **Water Level Sensors**: Analog or ultrasonic sensors for tank monitoring.
*   **Flow Meters**: Pulse-based sensors to measure water flow at critical points:
    *   Borehole to tank
    *   Tank to domestic supply
    *   Tank to farm
    *   Domestic supply to sales
*   **Environmental Sensors**: Optional sensors for temperature, humidity, etc.

## 2.3. Physical Deployment

*   **Remote Nodes**:
    *   Placed at sensor cluster points (e.g., by the borehole, water tank).
    *   Powered by a local solar panel and battery.
    *   The LoRa antenna should be positioned for optimal line-of-sight to the relay node.
*   **Relay Node**:
    *   Located near the Raspberry Pi, typically indoors.
    *   Connected to the Pi via USB.
    *   Positioned for the best possible LoRa reception from all remote nodes.
*   **Raspberry Pi**:
    *   Housed indoors, protected from dust and moisture.
    *   Connected to a UPS or battery backup for resilience.
    *   Connected to the local network via Ethernet or WiFi.
