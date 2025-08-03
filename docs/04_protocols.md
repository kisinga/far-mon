# 4. Communication Protocols

This document outlines the communication protocols used between the different components of the system. All packing and unpacking logic is centralized in the `/shared` directory for a single source of truth.

## 4.1. Remote to Relay (LoRa)

*   **Format**: Binary/Compact
*   **Content**:
    *   Timestamp
    *   Sensor ID
    *   Readings
    *   CRC for data integrity
*   **Notes**: The data is packed into a compact binary format to minimize airtime and power consumption.

## 4.2. Relay to Pi (Serial)

*   **Format**: Pluggable (Binary/JSON/CSV)
*   **Content**: The data received from the remote nodes is forwarded over serial.
*   **Notes**: The serial interface is designed to be flexible, allowing for different data formats to be used in the future.

## 4.3. Pi to ThingsBoard (MQTT/HTTP)

*   **Format**: JSON over MQTT or HTTP
*   **Content**: Telemetry data from the sensors.
*   **Notes**: This uses the standard ThingsBoard device API for data ingestion.

## 4.4. Configuration Downlink (Pi to Remote)

*   **Format**: Binary/JSON
*   **Content**:
    *   Device ID
    *   Configuration version
    *   Configuration fields
    *   CRC for data integrity
*   **Notes**: Configuration packets are sent from the Pi to the relay via serial, and then broadcast to the remote nodes via LoRa.
