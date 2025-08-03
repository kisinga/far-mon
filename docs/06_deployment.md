# 6. Deployment Guide

This guide provides a step-by-step process for deploying the farm monitoring system.

## 6.1. Deployment Sequence

1.  **Prepare Remote Sensor Nodes**:
    *   Flash the firmware from `edge/heltec-remote/` onto the remote sensor nodes.
    *   Configure the initial settings as required.

2.  **Prepare Relay Node**:
    *   Flash the firmware from `edge/heltec-relay/` onto the relay node.

3.  **Set Up Raspberry Pi**:
    *   Set up the Raspberry Pi according to the instructions in the [Raspberry Pi Setup](05_raspberry_pi.md) guide.
    *   Install and configure Docker, ThingsBoard, and the `relay-bridge` service.

4.  **Connect Components**:
    *   Connect the relay node to the Raspberry Pi via USB.
    *   Power on all components.
    *   For detailed wiring information, refer to the hardware documentation.

## 6.2. Security

*   **Tailscale VPN**:
    *   All remote access (dashboard, SSH, SFTP) is routed through the Tailscale VPN.
    *   Ensure that no ports are directly exposed to the public internet.
*   **Physical Security**:
    *   The Raspberry Pi and relay node should be housed in a secure, indoor location.
    *   Remote nodes should be installed in tamper-evident enclosures where possible.
