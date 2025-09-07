# 🏗️ Farm Monitoring System Architecture Guide

## Overview

This document describes the architecture of our Farm Monitoring System, designed to maximize maintainability and extensibility while following SOLID, KISS, and DRY principles. The system provides a clear, layered structure that makes it easy for engineers at all levels to understand, maintain, and extend the codebase.

The architecture is built around four distinct layers, each with a specific responsibility, creating a clean separation of concerns that promotes code reusability and testability.

---

## 🏛️ Architectural Overview

### The Four-Layer Architecture

Our system is organized into four distinct layers, each with a clear responsibility:

```
┌─────────────────────────────────────┐
│  4. Device Applications Layer       │  ← Specialized implementations
│  (RelayApplication, RemoteApplication) │
├─────────────────────────────────────┤
│  3. Application Framework Layer     │  ← Common orchestration
│  (CommonApplication)                │
├─────────────────────────────────────┤
│  2. System Services Layer           │  ← Specialized interfaces
│  (IBatteryService, IDisplayService) │
├─────────────────────────────────────┤
│  1. Core Systems Layer              │  ← Infrastructure foundation
│  (DeviceConfig, TaskManager)        │
└─────────────────────────────────────┘
```

### Understanding Through Analogy

Think of our system like a modern restaurant kitchen:

- **Device Applications** = Specialized chefs (sushi chef, pastry chef)
- **Application Framework** = Kitchen manager (coordinates all stations)
- **System Services** = Kitchen stations (grill station, prep station)
- **Core Systems** = Kitchen infrastructure (plumbing, electricity)

---

## 📚 Core Concepts

### 1. Dependency Injection (DI)

**Simple Explanation:**
Dependency Injection is like getting your lunch delivered instead of cooking it yourself. Instead of creating objects inside your class (tight coupling), you receive them from outside (loose coupling).

**Code Example:**
```cpp
// ❌ Tight coupling - Car creates its own engine
class Car {
    Engine engine;  // Car creates its own engine
};

// ✅ Loose coupling with DI - Engine given to car
class Car {
    Car(Engine* engine) : engine_(engine) {}  // Engine delivered
private:
    Engine* engine_;
};
```

**In Our System:**
```cpp
// Services are injected into the application
SystemServices services = SystemServices::create(display, wifi, battery, lora);
```

### 2. Interface Segregation

**Simple Explanation:**
Don't force classes to implement methods they don't need. Like not making a pizza delivery driver also cook the pizza.

**Code Example:**
```cpp
// ❌ One big interface - Chef forced to drive and code!
class Worker {
    virtual void cook() = 0;
    virtual void drive() = 0;  // Chef doesn't drive!
    virtual void code() = 0;   // Chef doesn't code!
};

// ✅ Separate interfaces - Each role has only what it needs
class Chef {
    virtual void cook() = 0;
};

class Driver {
    virtual void drive() = 0;
};

class Developer {
    virtual void code() = 0;
};
```

**In Our System:**
```cpp
// Each service has only the methods it needs
class IBatteryService {
    virtual uint8_t getBatteryPercent() const = 0;
    virtual bool isCharging() const = 0;
    // No display methods here!
};
```

---

## 🔧 Layer-by-Layer Deep Dive

## Layer 1: Core Systems (Foundation)

### Device Configuration System

**Purpose:** Defines the "personality" of each device type.

```cpp
// lib/device_config.h
enum class DeviceType : uint8_t {
    Relay = 1,   // Central hub with WiFi
    Remote = 2   // Sensor node
};

struct DeviceConfig {
    const char* deviceId;           // Unique identifier ("01", "03")
    DeviceType deviceType;          // What kind of device
    uint32_t heartbeatIntervalMs;   // How often to signal alive
    bool enableDisplay;            // Has OLED screen?
    bool enableDebug;              // Log debug info?
    uint32_t displayUpdateIntervalMs; // Screen refresh rate
    uint32_t loraTaskIntervalMs;    // LoRa communication frequency
};
```

**Benefits:**
- **Single Source of Truth:** All device settings in one place
- **Type Safety:** Compiler prevents invalid configurations
- **Easy Modification:** Change device behavior without touching code

### Task Management System

**Purpose:** Handles timing and execution of periodic tasks.

```cpp
// lib/task_manager.h
class TaskManager {
public:
    // Register a task to run every N milliseconds
    bool registerTask(const std::string& name,
                     std::function<void(CommonAppState&)> callback,
                     uint32_t intervalMs);

    // Run all registered tasks
    void update(CommonAppState& state);
};
```

**Benefits:**
- **Centralized Timing:** No more `millis()` calculations in application code
- **Consistent Patterns:** All tasks follow the same registration pattern
- **Easy Debugging:** Tasks can be enabled/disabled individually

---

## 🔌 Layer 2: System Services (Specialized Tools)

### Service Interface Pattern

Each service follows a consistent pattern:
1. **Interface** (abstract base class)
2. **Concrete Implementation**
3. **Clean contract** with minimal methods

### Battery Service Example

```cpp
// Service Interface
class IBatteryService {
public:
    virtual ~IBatteryService() = default;
    virtual void update(uint32_t nowMs) = 0;
    virtual uint8_t getBatteryPercent() const = 0;
    virtual bool isCharging() const = 0;
};

// Concrete Implementation
class BatteryService : public IBatteryService {
public:
    explicit BatteryService(BatteryMonitor::BatteryMonitor& monitor)
        : batteryMonitor(monitor) {}

    void update(uint32_t nowMs) override {
        batteryMonitor.updateChargeStatus(nowMs);
    }

    uint8_t getBatteryPercent() const override {
        uint8_t percent = 255;
        batteryMonitor.readPercent(percent);
        return percent;
    }

    bool isCharging() const override {
        return batteryMonitor.isCharging();
    }

private:
    BatteryMonitor::BatteryMonitor& batteryMonitor;
};
```

**Why This Pattern?**
- **Testability:** Can mock the interface for unit tests
- **Flexibility:** Easy to swap implementations
- **Maintainability:** Changes to battery logic don't affect other services

### Service Container Pattern

```cpp
// lib/system_services.h
struct SystemServices {
    std::unique_ptr<IBatteryService> battery;
    std::unique_ptr<IDisplayService> display;
    std::unique_ptr<IWifiService> wifi;
    std::unique_ptr<ILoRaService> lora;

    // Factory method - creates all services at once
    static SystemServices create(OledDisplay& oled, /* ... other deps */);
};
```

**Benefits:**
- **Dependency Injection:** All services provided to consumers
- **Resource Management:** Smart pointers handle cleanup
- **Consistency:** Same creation pattern for all services

---

## 🎯 Layer 3: Application Framework (Orchestrator)

### Common Application Base Class

```cpp
// lib/common_app.h
class CommonApplication {
public:
    void initialize();  // Sets up everything
    void run();        // Main application loop

protected:
    // Hooks for device-specific customization
    virtual void setupDeviceSpecific() = 0;     // "I'm special"
    virtual void registerDeviceTasks() = 0;     // "I need these tasks"
    virtual void setupDeviceConfig() = 0;       // "This is my personality"

private:
    // Framework handles all complexity
    void initializeHardware();
    void initializeServices();
    void initializeTasks();
    void initializeDisplay();
};
```

**The Framework Pattern:**
1. **Common Setup:** Hardware, services, tasks (80% of work)
2. **Device Customization:** Hooks for specific behavior (20% of work)
3. **Unified Interface:** Same `initialize()`/`run()` for all devices

---

## 🎨 Layer 4: Device Applications (Specializations)

### Relay Application Example

```cpp
// relay/relay_app.h
class RelayApplication : public CommonApplication {
public:
    RelayApplication();

protected:
    void setupDeviceConfig() override {
        config = createRelayConfig("01");  // I'm device "01"
    }

    void setupDeviceSpecific() override {
        // I need WiFi and peer monitoring
        setupWifi();
        setupDisplayProviders();
    }

    void registerDeviceTasks() override {
        // I monitor peers and handle WiFi
        getTaskManager().registerTask("peer_monitor",
            [this](CommonAppState& state) { monitorPeers(); },
            config.peerMonitorIntervalMs);
    }

private:
    RelayConfig config;
    void monitorPeers();  // My special peer monitoring logic
};
```

### Remote Application Example

```cpp
// remote/remote_app.h
class RemoteApplication : public CommonApplication {
protected:
    void setupDeviceConfig() override {
        config = createRemoteConfig("03");  // I'm device "03"
    }

    void setupDeviceSpecific() override {
        // I read sensors and send telemetry
        pinMode(config.analogInputPin, INPUT);
        setupTelemetryConfig();
    }

    void registerDeviceTasks() override {
        // I read sensors and report data
        getTaskManager().registerTask("analog_read",
            [this](CommonAppState& state) { readAnalogSensor(); },
            config.analogReadIntervalMs);

        getTaskManager().registerTask("telemetry_report",
            [this](CommonAppState& state) { sendTelemetryReport(); },
            config.telemetryReportIntervalMs);
    }

private:
    RemoteConfig config;
    float analogVoltage = 0.0f;
};
```

---

## 🛠️ Extension Guide

### Adding a New Device Type

**Step 1:** Create device configuration
```cpp
// lib/device_config.h
struct GreenhouseMonitorConfig : DeviceConfig {
    bool enableMoistureSensors;
    bool enableTemperatureSensors;
    uint32_t sensorReadIntervalMs;
};
```

**Step 2:** Create device application
```cpp
// greenhouse/greenhouse_app.h
class GreenhouseMonitorApp : public CommonApplication {
protected:
    void setupDeviceConfig() override {
        config = createGreenhouseConfig("05");
    }

    void setupDeviceSpecific() override {
        setupMoistureSensors();
        setupTemperatureSensors();
    }

    void registerDeviceTasks() override {
        registerSensorTasks();
        registerIrrigationTasks();
    }
};
```

**Step 3:** Create main application file
```cpp
// greenhouse/greenhouse.ino
#include "greenhouse_app.h"
GreenhouseMonitorApp greenhouseApp;

void setup() {
    greenhouseApp.initialize();
}

void loop() {
    greenhouseApp.run();
}
```

### Adding a New Service

**Step 1:** Create service interface
```cpp
class IBluetoothService {
public:
    virtual ~IBluetoothService() = default;
    virtual bool isConnected() const = 0;
    virtual void sendData(const uint8_t* data, size_t len) = 0;
};
```

**Step 2:** Create concrete implementation
```cpp
class BluetoothService : public IBluetoothService {
public:
    explicit BluetoothService(BluetoothManager& manager)
        : bluetoothManager(manager) {}

    bool isConnected() const override {
        return bluetoothManager.connected();
    }
};
```

**Step 3:** Add to service container
```cpp
struct SystemServices {
    std::unique_ptr<IBluetoothService> bluetooth;

    static SystemServices create(/* ... params */, BluetoothManager& bt) {
        SystemServices services;
        services.bluetooth = std::make_unique<BluetoothService>(bt);
        return services;
    }
};
```

---

## 📋 Best Practices

### Service Interface Design
```cpp
// ✅ Good: Minimal, focused interface
class IService {
    virtual void essentialMethod() = 0;
    // Only what the service absolutely needs
};

// ❌ Avoid: Bloated interface
class IService {
    virtual void method1() = 0;
    virtual void method2() = 0;
    virtual void method3() = 0;  // Forced on all implementations
};
```

### Task Registration Patterns
```cpp
// ✅ Good: Descriptive names and logical grouping
taskManager.registerTask("battery_monitor",
    [this](CommonAppState& state) { checkBattery(); }, 5000);

taskManager.registerTask("sensor_telemetry",
    [this](CommonAppState& state) { sendSensorData(); }, 2000);
```

### Configuration Management
```cpp
// ✅ Good: Factory functions for complex setup
RelayConfig createRelayConfig(const char* deviceId) {
    RelayConfig config;
    config.deviceId = deviceId;
    config.enableWifi = true;  // Relay-specific defaults
    return config;
}
```

---

## 📚 Key Takeaways

1. **Think in Layers:** Foundation → Services → Framework → Applications
2. **Use Contracts:** Interfaces define clear boundaries between components
3. **Inject Dependencies:** Don't create, receive what you need
4. **Single Responsibility:** Each class should have one clear purpose
5. **Test Interfaces:** Mock services to test components in isolation
6. **Configuration First:** Define device personality before implementation
7. **Consistent Patterns:** Follow established conventions for maintainability

This architecture transforms complex embedded development into an intuitive, maintainable system. New features can be added with minimal disruption, and the codebase remains clean and understandable for both junior and senior engineers.

**Remember:** Good architecture is like a well-organized kitchen - everything has its place, tools are easy to find, and adding new dishes (features) is straightforward! 🍳✨
