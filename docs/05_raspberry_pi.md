# 5. Raspberry Pi Setup

The Raspberry Pi serves as the edge server for the farm monitoring system. It runs a unified Docker container for all custom services, which can be configured to communicate with any ThingsBoard instance. This entire setup is managed by a self-hosted instance of Coolify.

## 5.1. Architecture

All custom code for the Raspberry Pi is managed in a single Go project and deployed as a single Docker container. This simplifies deployment and management. The main service is the `relay-bridge`, which is designed to be modular and extensible.

The container communicates with a ThingsBoard API to send telemetry data and receive configuration updates. The ThingsBoard instance can be local or remote, and is configured via a configuration file.

### 5.1.1. Coolify Management

The Docker container is deployed and managed using a self-hosted Coolify instance. This provides a simple, git-based workflow for deploying updates to the edge device.

## 5.2. Project Structure

The code for the Raspberry Pi services is located in the `edge/pi/` directory.

| File/Folder               | Description                                      |
| :------------------------ | :----------------------------------------------- |
| `src/`                    | Go source code for the services.                 |
| `src/cmd/relay-bridge/`   | Main application for the `relay-bridge`.         |
| `src/pkg/`                | Shared Go packages (`config`, `thingsboard`, `serial`). |
| `config.yaml`             | Default configuration file for the services.     |
| `Dockerfile`              | Dockerfile for building the services container.  |
| `docker-compose.yml`      | Docker Compose file for local development.       |


## 5.3. Configuration

The `relay-bridge` service has a default configuration that is compiled into the application. This default configuration can be overridden by a `config.yaml` file or by environment variables.

The configuration loading order is as follows:
1.  Compiled-in default values.
2.  Values from a `config.yaml` file (if present). This file is looked for in `/app/config.yaml` inside the container.
3.  Environment variables (e.g., `THINGSBOARD_HOST`).

To customize the configuration, you can mount a `config.yaml` file into the container at `/app/config.yaml`.

Here is an example `config.yaml`:
```yaml
thingsboard:
  host: "your-thingsboard-host"
  port: 8080
  token: "YOUR_THINGSBOARD_TOKEN"
```

## 5.4. Deployment

To deploy the services, push your changes to the `main` branch. Coolify will automatically detect the changes, build the Docker image, and deploy it to the Raspberry Pi.

## 5.5. ThingsBoard

The `relay-bridge` no longer depends on a local ThingsBoard instance. It can be configured to work with any ThingsBoard instance, whether it's running on the same device, on your local network, or in the cloud.
