# Farm Infrastructure & High Impact Metrics Integration
This repository contains the source code, configuration, and documentation for my autonomous farm monitoring and control system. The system is designed as a modular, resilient, and extensible foundation. Over time, it has evolved into a data-driven decision engine, supporting high-impact measurements and paving the way for deep integration with record-keeping (ERPNext and beyond).

## ✨ Purpose Shift: From Infrastructure to Intelligence
I initially built this system for resilient, modular, and low-maintenance operation. My ongoing goal is to convert raw sensor and operational data into actionable interventions—decisions that directly influence yield, resource efficiency, and water conservation. This isn't just automation; it’s farm-level intelligence under constraint, built for expansion.

## High-Level Structure
- **Modular Design**: Each device and logical function is isolated and swappable.
- **Resilient**: Designed for low-maintenance, high-resilience edge deployment.
- **Extensible**: Future-proof, built to integrate additional sensors, analytics, and eventually, ERP-linked record-keeping.

### Directory Overview
| Folder | Purpose |
| :--- | :--- |
| `docs/` | Contains all documentation for the system. |
| `edge/` | Code for on-site devices (remote, relay, Pi). |
| `core/` | Cloud/home-side code (future/optional). |
| `shared/` | Protocols, config schemas, and serialization libs. |

## 🛠️ Current Capabilities (Deployed Stack, WIP)
| Component | Role | Sensors/Methods |
| :--- | :--- | :--- |
| Remote Heltec Node | Sensor interface and uplink | Sunverter (solar), tank level (ultrasonic), flow meters, soil moisture (planned) |
| Relay Node | LoRa-to-Serial bridge | Sends uplinks to Raspberry Pi and distributes downlinks |
| Raspberry Pi Edge Node | Data aggregation and config push | Serial listener, LoRa command interface, local logging and daemon control |
| Dynamic Config System | Remote firmware reconfig | LoRa-based JSON packet to ESP32 NVS |
| NVS Storage | On-device persistence | Retains sensor and radio config through reboots |

### Built-in Flexibility:
- Extend `SensorConfig` for new sensor types
- Adjust polling and payload priority
- Push alerts or set flags using downstream LoRa commands

## 📊 High-Impact Measurements (Prioritized)
| Category | What I Measure or Will Measure | Sensor/Method |
| :--- | :--- | :--- |
| Water | Tank level, borehole output, rainfall, soil moisture | HC-SR04 ultrasonic, tipping bucket, resistive probe |
| Goats | Monthly weight, health status, births | Manual weighboard or RFID gate scale |
| Fodder Plots | Harvest timestamp, regrowth rate, plot-level moisture | Manual logs + sensor probe |
| Beehives | Temperature, humidity, entry/exit activity | Thermal probe, IR motion counter, mic (optional) |
| Vegetables | Irrigation timing, harvest yield, pest sightings | Mobile form entries |
| System Health | Pump runtime, solar yield, tank overflow/leakage | Pulse counter, shunt sensor, float valve |
| Weather | Rainfall, temperature, humidity | Local station (or API fallback) |

## 🚀 Immediate Action Items (MVP Build Tier)
| Component | Tool | Function |
| :--- | :--- | :--- |
| Tank Monitoring | HC-SR04 + remote node | Prevent tank overflows, monitor depletion rate |
| Rain Gauge | Tipping bucket | Inform irrigation suppression logic and dry period prediction |
| Goat Weighing | Manual weighboard | Track average daily gain and feed response |
| Soil Moisture | 2–3 capacitive probes | Fine-tune irrigation schedules |
| Crop Logging | KoboToolbox or web form | Record transplant/harvest/infection events |
| Hive Status | Temp/humidity sensor | Predict swarming or colony stress before failure |

All data is serialized using my `SensorConfig` and piped into the Raspberry Pi. From there, timed services aggregate, alert, or trigger actions.

## ⚖️ Near-Term Additions (Building on My Stack)
| Add-On | Hook Point | Objective |
| :--- | :--- | :--- |
| Sunverter 4b Integration | Remote Node + TTL-to-RS485 | Monitor detailed solar system metrics (yield, battery status) |
| Pump Runtime Counter | Flow meter + log daemon | Detect inefficient cycles or dry-running |
| Zonal LoRa Nodes | One per field block | Enable per-plot decisions (moisture, temp, irrigation) |
| Dashboard | Locally hosted web UI | Onsite awareness of water, climate, and livestock metrics |
| Rule Engine | Pi daemon (thresholds) | Run logic (e.g., rain < X + tank > 30% → irrigate plot 2) |

## 📆 Future Roadmap: ERPNext Integration
**Vision**:
Integrate my entire sensor and event-logging system into a farm ERP (ERPNext), enabling unified record-keeping and analytics.

**Aspirations**:
- Full livestock and crop log integration
- Direct syncing of sensor data to a time-series backend
- Offline-first mobile data entry
- Auto-generated reports for production, consumption, and water use
- Link economics directly to field conditions

**Requirements**:
- Time-series ingestion service (Pi → ERPNext adapter or message broker)
- Mobile forms with offline sync
- REST/MQTT bridge for real-time dashboards or warnings
- Device registry in ERPNext for per-sensor metadata
- Integration hooks for goat, crop, and inventory records

## Getting Started
To understand the system architecture, start with the [System Overview](docs/01_overview.md).

For detailed documentation, see the [main documentation page](docs/README.md).

For deployment instructions, refer to the [Deployment Guide](docs/06_deployment.md).
