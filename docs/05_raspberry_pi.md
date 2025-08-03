# 5. Raspberry Pi Setup

The Raspberry Pi serves as the edge server for the farm monitoring system. It runs the ThingsBoard platform, the `relay-bridge` service, and provides secure remote access.

## 5.1. Roles and Responsibilities

*   **ThingsBoard**: Provides the local telemetry dashboard, data storage, and rule engine.
*   **Relay Bridge**: A service that transfers data between the relay node (serial) and ThingsBoard (MQTT/HTTP).
*   **VPN Entry Point**: Uses Tailscale for secure, remote access to the dashboard and SSH.
*   **Logging and Health**: Manages field logging, configuration distribution, and service health monitoring.

## 5.2. ThingsBoard Deployment

ThingsBoard runs as a Docker container on the Pi.

### Getting Started

1.  Install Docker and Docker Compose on the Raspberry Pi.
2.  Copy the provided `docker-compose.yml` file to the `edge/pi/thingsboard/` directory.
3.  Start ThingsBoard in detached mode:
    ```sh
    docker-compose up -d
    ```

### Security

The ThingsBoard instance is secured by Tailscale and is not accessible from the public internet.

## 5.3. Relay Bridge

The `relay-bridge` is a custom service that:

*   Reads sensor data from the relay node via the serial port.
*   Parses the data and forwards it to the ThingsBoard telemetry API.
*   Listens for configuration updates from ThingsBoard and sends them to the relay node.
*   Is located in the `edge/pi/relay-bridge/` directory.
