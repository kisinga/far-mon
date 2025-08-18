// Header-only cooperative task scheduler for Arduino/ESP32
// - Generic over device-defined State type
// - Non-blocking, interval-driven tasks
// - Wrap-around safe timing

#pragma once
#include <Arduino.h>

template <typename State>
using TaskCallback = void (*)(State &);

template <typename State, size_t MaxTasks>
class TaskScheduler {
 public:
  TaskScheduler() : taskCount(0) {}

  bool registerTask(const char *name, TaskCallback<State> callback, uint32_t intervalMs) {
    if (taskCount >= MaxTasks) {
      return false;
    }
    Task &t = tasks[taskCount++];
    t.name = name;
    t.callback = callback;
    t.intervalMs = intervalMs;
    t.nextRunMs = millis() + intervalMs;
    t.enabled = true;
    return true;
  }

  void setEnabled(const char *name, bool enabled) {
    for (size_t i = 0; i < taskCount; i++) {
      if (tasks[i].name && name && strcmp(tasks[i].name, name) == 0) {
        tasks[i].enabled = enabled;
        return;
      }
    }
  }

  void tick(State &state) {
    const uint32_t now = millis();
    for (size_t i = 0; i < taskCount; i++) {
      Task &t = tasks[i];
      if (!t.enabled) continue;
      // Run when now >= nextRunMs (wrap-safe)
      if ((int32_t)(now - t.nextRunMs) >= 0) {
        t.callback(state);
        // Schedule next
        const uint32_t scheduled = t.nextRunMs + t.intervalMs;
        if ((int32_t)(now - scheduled) >= 0) {
          // We are late; reset relative to now to prevent tight catch-up loops
          t.nextRunMs = now + t.intervalMs;
        } else {
          t.nextRunMs = scheduled;
        }
      }
    }
  }

 private:
  struct Task {
    const char *name;
    TaskCallback<State> callback;
    uint32_t intervalMs;
    uint32_t nextRunMs;
    bool enabled;
  };

  Task tasks[MaxTasks];
  size_t taskCount;
};


