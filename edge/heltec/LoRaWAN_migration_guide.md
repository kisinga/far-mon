# LoRaWAN Migration: Architectural Vision & Strategy

## 1. Motivation

Our existing custom LoRa protocol is facing significant reliability challenges, which impacts data integrity and operational stability. To address this, we are migrating to the LoRaWAN standard. This will provide a robust, industry-proven foundation, improving reliability and unlocking advanced capabilities for remote management and future scalability.

## 2. Target Architecture: The Long-Term Vision

The end-state architecture will be a fully compliant LoRaWAN network, composed of three core components:

- **LoRaWAN End Devices**: Our remote sensor nodes will operate as standard LoRaWAN devices. This ensures interoperability and allows for the future addition of off-the-shelf LoRaWAN sensors.
- **LoRaWAN Gateway**: A professional-grade gateway (e.g., a Raspberry Pi with a LoRaWAN HAT) will be deployed to receive data from all end devices within its range.
- **Network & Application Server**: A LoRaWAN Network Server, such as ChirpStack, will manage the network. It will handle device authentication, message decryption, Adaptive Data Rate (ADR), and forwarding data to our application backend via MQTT.

This architecture decouples the physical layer (LoRa) from the application layer, creating a scalable and maintainable system.

## 3. Transitional Strategy: A Phased Approach

To de-risk the migration and enable parallel development, we will adopt a two-phase transitional strategy. The core principle is to make the `remote` node LoRaWAN-compliant first, even before the new gateway hardware is deployed.

### Phase 1: Firmware-First Migration

This phase focuses on software and firmware changes, using existing hardware.

- **Remote Node Refactoring**:

  - The `remote` node's firmware will be re-architected to use a standard LoRaWAN stack (Class A). It will communicate as if it were talking to a real LoRaWAN gateway.
  - An AT command interface will be implemented, allowing for remote configuration and control via LoRaWAN downlinks. This is crucial for maintainability.

- **Relay Node as a Temporary "Packet Forwarder"**:
  - The `relay` node will be modified to act as a minimal, non-compliant LoRaWAN packet forwarder.
  - Its sole responsibility is to receive the raw LoRaWAN packets from the `remote` node, extract the application payload, and forward it upstream via its existing serial connection.
  - **Crucially, this component is temporary and designed to be discarded.** It allows the `remote` node to be developed and tested against a LoRaWAN-like target without waiting for the gateway hardware. This makes the final gateway a "drop-in" replacement from the remote's perspective.

### Phase 2: Gateway Deployment

This phase completes the migration by deploying the new hardware.

- **Gateway Installation**: The LoRaWAN Gateway hardware will be installed and configured with the Network Server (e.g., ChirpStack).
- **Relay Decommissioning**: The temporary `relay` node will be decommissioned and removed.
- **Data Path Re-Routing**: The backend services will be updated to consume data directly from the Network Server's MQTT feed, which will provide decrypted and structured data.

## 4. Architectural Benefits & Outcomes

This migration will yield significant benefits:

- **Enhanced Reliability**: Moves from a brittle custom protocol to a robust, self-healing, and professionally maintained standard.
- **Remote Management**: Enables remote device configuration and debugging via the AT command interface and LoRaWAN downlinks.
- **Scalability & Interoperability**: The system will be able to support a larger number of devices and seamlessly integrate third-party LoRaWAN-compliant sensors.
- **Reduced Maintenance**: Offloads the complexity of managing a wireless protocol to a dedicated Network Server.
