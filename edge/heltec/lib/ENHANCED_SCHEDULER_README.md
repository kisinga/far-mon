# Enhanced Job Scheduler

This module introduces a robust job scheduler based on FreeRTOS:

- FreeRTOS-backed only: `RtosTaskScheduler<State, N>` in `rtos_scheduler.h`

`TaskManager` is now RTOS-only on ESP32 and starts the scheduler automatically in app initialization.

## Why

- Better timing accuracy and jitter handling under load
- Non-blocking main loop: tasks execute in a dedicated RTOS task
- Same simple API: name, callback, interval

## Usage

Most apps should just use `TaskManager` as before.

```cpp
TaskManager taskManager; // RTOS-only on ESP32
taskManager.registerTask("foo", [](CommonAppState& s){ /* ... */ }, 1000);
taskManager.start(appState); // no-op for cooperative
```

The cooperative scheduler has been removed from the default path for ESP32 targets.

## Notes

- Runtime task registration after `start()` is disabled for the RTOS scheduler.
- Use `setTaskEnabled(name, enabled)` to control tasks dynamically.
- Callbacks must be non-blocking and return quickly.


