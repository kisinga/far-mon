# 🏗️ Farm Monitoring System Architecture Guide (v2.0)

## Overview

This document describes the refactored architecture of the Farm Monitoring System. The new design emphasizes modularity, clear separation of concerns, and testability, adhering to SOLID principles. It is organized into a five-layer architecture that promotes a clean, intuitive, and maintainable codebase, even with the constraint of a flat `lib` directory structure.

---

## 🏛️ Architectural Overview

### The Five-Layer Architecture

Our system is organized into five distinct layers, each with a clear responsibility. The flat directory structure is managed through a consistent file naming convention:

```
┌───────────────────────────────────────────┐
│  5. Application Layer (`<device>_app.h`)  │  ← Device-specific business logic
│     (RelayApplication, RemoteApplication) │
├───────────────────────────────────────────┤
│  4. Services Layer (`svc_*.h`)            │  ← High-level features (UI, Comms)
│     (UiService, CommsService)             │
├───────────────────────────────────────────┤
│  3. UI Components (`ui_*.h`)              │  ← Reusable UI elements and layouts
│     (ui_button, ui_layout)                │
├───────────────────────────────────────────┤
│  2. Hardware Abstraction Layer (`hal_*.h`)│  ← Hardware-specific interfaces
│     (IDisplayHal, ILoRaHal)               │
├───────────────────────────────────────────┤
│  1. Core Layer (`core_*.h`)               │  ← Foundational utilities
│     (core_config, core_scheduler)         │
└───────────────────────────────────────────┘
```

### Core Principles

- **Dependency Injection (DI):** Components receive their dependencies from an external source rather than creating them internally. This is managed in the "composition root" within the main `.ino` files.
- **Interface-Based Design:** Services and HALs are defined by interfaces (abstract base classes), allowing for interchangeable implementations and easier testing.
- **Clear Naming Conventions:** The flat `lib` directory is organized by prefixes (`core_`, `hal_`, `svc_`, `ui_`), making the architecture evident from the file structure.

---

## 🔧 Layer-by-Layer Deep Dive

### Layer 1: Core Layer (`core_*.h`)

This layer provides the foundational building blocks for the entire system.

- `core_config.h/.cpp`: A centralized, factory-based configuration system for all device types.
- `core_scheduler.h/.cpp`: An RTOS-backed task scheduler for managing periodic tasks.
- `core_logger.h`: A header-only logging utility with support for different log levels.
- `core_system.h/.cpp`: Orchestrates the system-wide initialization sequence.

### Layer 2: Hardware Abstraction Layer (HAL) (`hal_*.h`)

This layer decouples the application from the hardware. It consists of pure virtual interfaces and concrete implementations that wrap the low-level drivers.

- `hal_display.h/.cpp`: Interface (`IDisplayHal`) and implementation for the OLED display.
- `hal_lora.h/.cpp`: Interface (`ILoRaHal`) and implementation for the LoRa radio.
- `hal_wifi.h/.cpp`: Interface (`IWifiHal`) and implementation for the WiFi module.
- `hal_battery.h/.cpp`: Interface (`IBatteryHal`) and implementation for the battery monitor.

### Layer 3: UI Components (`ui_*.h`)

This layer contains reusable UI elements and layouts.

- `ui_element.h`: The base class for all UI elements.
- `ui_layout.h`: The base class for all layout containers.
- Other files (`ui_button.h`, `ui_text_element.h`, etc.) define specific, reusable UI components.

### Layer 4: Services Layer (`svc_*.h`)

This layer provides high-level services built on top of the HAL and Core layers.

- `svc_ui.h/.cpp`: The UI Service, responsible for managing the screen, layouts, and rendering.
- `svc_comms.h/.cpp`: The Communication Service, which manages all communication transports and routes messages between them.
- Other service files (`svc_battery.h`, `svc_wifi.h`, etc.) provide focused services for specific domains.

### Layer 5: Application Layer (`<device>_app.h`)

This is the top layer, containing the specific "business logic" for each device.

- `remote_app.h/.cpp`: The implementation for the Remote sensor node.
- `relay_app.h/.cpp`: The implementation for the Relay hub.
- The main `.ino` files (`remote.ino`, `relay.ino`) are now extremely simple, containing only the Arduino `setup` and `loop` functions, which delegate to the respective application class.

---

## 🛠️ Extension Guide

### Adding a New Service

1.  **Create the HAL (if needed):** If the service requires new hardware, create a new `hal_*.h` and `hal_*.cpp`.
2.  **Define the Service Interface:** Create a `svc_*.h` file with an `IService` interface.
3.  **Implement the Service:** Create a `svc_*.cpp` file with a concrete implementation of the service, depending on the necessary HALs.
4.  **Inject the Service:** In the main `.ino` file, instantiate the new service and pass it to the components that need it.

---

## 📚 Key Takeaways

1.  **Clear Layers:** The five-layer architecture provides a clear and logical separation of concerns.
2.  **Dependency Injection is Key:** The composition root in the `.ino` files manages the object graph, promoting loose coupling.
3.  **Interfaces are Contracts:** The HAL and service interfaces define the boundaries between layers, allowing for flexibility and testability.
4.  **Naming Conventions Matter:** The file naming convention makes the architecture easy to understand at a glance.

This new architecture provides a robust, scalable, and maintainable foundation for the Farm Monitoring System.
