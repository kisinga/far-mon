# Farm Infrastructure

This repository contains the source code, configuration, and documentation for an autonomous farm monitoring and control system.

## High-Level Structure

*   **Modular Design**: Each device and logical function is isolated and swappable.
*   **Resilient**: Designed for low-maintenance, high-resilience edge deployment.
*   **Extensible**: Future-proof and easy to extend.

## Directory Overview

| Folder      | Purpose                                           |
| :---------- | :------------------------------------------------ |
| `docs/`     | Contains all documentation for the system.        |
| `edge/`     | Code for on-site devices (remote, relay, Pi).     |
| `core/`     | Cloud/home-side code (future/optional).           |
| `shared/`   | Protocols, config schemas, and serialization libs.|

## Getting Started

1.  To understand the system architecture, start with the [System Overview](docs/01_overview.md).
2.  For detailed documentation, see the [main documentation page](docs/README.md).
3.  For deployment instructions, refer to the [Deployment Guide](docs/06_deployment.md).
