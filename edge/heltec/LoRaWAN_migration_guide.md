## LoRaWAN Migration: Architectural Guide

### 1. Executive Summary

Our current custom LoRa protocol has proven to be unreliable, creating significant operational challenges. This document outlines the architectural strategy for migrating our farm monitoring system to the industry-standard LoRaWAN protocol. This migration will enhance system reliability, enable remote device management, and position the platform for future expansion with standard off-the-shelf LoRaWAN devices.

The migration is designed to be executed in phases to ensure zero downtime and allow for incremental testing and deployment.

### 2. Chosen Technology Stack

The new architecture will be built on a robust, open-source stack:

- **LoRaWAN Network Server**: **ChirpStack** will be used to manage the LoRaWAN network, including device authentication, data routing, and adaptive data rates.
- **Message Broker**: **Mosquitto (MQTT)** will continue to serve as the messaging backbone, decoupling the network server from our data processing applications.
- **Automation & Processing**: **Node-RED** will be used for processing and visualizing the telemetry data received via MQTT.

This stack was chosen for its native LoRaWAN support, excellent MQTT integration, open-source licensing, and flexibility.

### 3. Phased Migration Strategy

The migration is broken into two main phases to de-risk the transition and separate firmware development from hardware deployment.

#### Phase 1: Remote-First Migration (Pre-Hardware Arrival)

This phase focuses on upgrading the remote sensor nodes to be LoRaWAN-compliant while the existing relay hardware is still in place. This allows for firmware development and testing to proceed in parallel with hardware procurement.

**Architectural Goals:**

1.  **Firmware Upgrade**: The firmware on the remote Heltec devices will be refactored to replace the custom LoRa implementation with a standard LoRaWAN stack (Class A).
2.  **Remote Management**: Implement support for AT-style commands received via LoRaWAN downlinks. This is critical for remote debugging, configuration changes (e.g., sensor poll interval), and maintenance without requiring physical access.
3.  **Temporary Gateway Emulation**: The existing Relay node's firmware will be modified to function as a minimal, temporary packet forwarder. It will not be a compliant LoRaWAN gateway. Its sole responsibility is to capture LoRaWAN packets from the remote nodes and forward the raw payloads to the Bridge via serial. This is a **throwaway component** designed to bridge the gap until the new hardware arrives.
4.  **Bridge Adaptation**: The Bridge script will be updated to parse the incoming serial data from the Relay and forward the extracted sensor payloads to MQTT.

At the end of this phase, all remote devices will be speaking LoRaWAN, but the backend infrastructure will still be using the legacy relay hardware as a temporary bridge.

#### Phase 2: Full LoRaWAN Gateway Deployment (Hardware Arrival)

This phase replaces the temporary relay solution with a production-grade LoRaWAN gateway.

**Architectural Goals:**

1.  **Hardware Deployment**: A Raspberry Pi equipped with a generic LoRaWAN gateway HAT (e.g., SX1302-based) will be deployed. This hardware will be managed by ChirpStack.
2.  **ChirpStack Deployment**: The full ChirpStack stack (Network Server, Gateway Bridge) will be deployed on the Raspberry Pi using Docker containers.
3.  **Relay Decommissioning**: The Heltec Relay device will be physically removed from the system.
4.  **Data Path Re-routing**: The Bridge component will be reconfigured. Instead of listening for serial data, it will now subscribe directly to the MQTT topics published by the ChirpStack server. ChirpStack will handle the decoding of LoRaWAN packets and publish decrypted sensor data in a structured format (e.g., JSON).

Upon completion of this phase, the system will be operating on a standard, fully compliant LoRaWAN architecture.

### 4. Architectural Benefits

This phased approach provides several key advantages:

- **Zero Downtime**: The existing system remains operational throughout the migration process.
- **Incremental Rollout**: Changes can be developed and tested in isolated stages (firmware first, then hardware).
- **Hardware Agnostic**: The architecture is not tied to a specific LoRaWAN HAT. Any ChirpStack-compatible gateway hardware can be used.
- **Enhanced Reliability**: Eliminates the maintenance burden and flakiness of a custom protocol.
- **Advanced Capabilities**: Unlocks professional features like Adaptive Data Rate (ADR), secure communication, and remote device management.

### 5. Hardware Compatibility

The architecture is designed to work with any Raspberry Pi HAT that is supported by the ChirpStack Gateway Bridge. This includes a wide variety of common modules based on Semtech chipsets.

**Examples of Supported HATs:**

- Waveshare SX1302/SX1303 Series
- Pi Supply LoRa Gateway HAT
- RAKwireless Gateway HATs (RAK831, RAK2245, etc.)
- Dragino LoRa Gateway HATs
