// Task Manager - Simplified task management with common patterns
// Provides clean interfaces for task registration and execution

#pragma once

#include <functional>
#include <vector>
#include <string>
#include "rtos_scheduler.h"

// Generic app state for common tasks
struct CommonAppState {
    uint32_t nowMs = 0;
    bool heartbeatOn = false;
    // Add other common state variables as needed
};

// Task definition
struct TaskDefinition {
    std::string name;
    std::function<void(CommonAppState&)> callback;
    uint32_t intervalMs;
    bool enabled = true;
};

// Task Manager - manages common and custom tasks
class TaskManager {
public:
    explicit TaskManager(size_t maxTasks = 16)
        : maxTasks(maxTasks) {
        tasks.reserve(maxTasks);
    }

    // Register a custom task
    bool registerTask(const std::string& name,
                      std::function<void(CommonAppState&)> callback,
                      uint32_t intervalMs) {
        if (tasks.size() >= maxTasks) return false;

        TaskDefinition task;
        task.name = name;
        task.callback = callback;
        task.intervalMs = intervalMs;
        tasks.push_back(task);

        return rtosScheduler.registerTask(tasks.back().name.c_str(), tasks.back().callback, intervalMs);
    }

    // Update task execution
    void update(CommonAppState& state) {
        // RTOS runs callbacks in its own task; keep loop responsive
        (void)state;
        return;
    }

    // Enable/disable tasks
    void setTaskEnabled(const std::string& name, bool enabled) {
        rtosScheduler.setEnabled(name.c_str(), enabled);
    }

    // Start scheduler
    bool start(CommonAppState& state) {
        return rtosScheduler.start(state, "job-scheduler");
    }

    // Declared here, implemented in .cpp
    void registerCommonTasks(const DeviceConfig& config, SystemServices& services);

private:
    size_t maxTasks;
    std::vector<TaskDefinition> tasks;
    RtosTaskScheduler<CommonAppState, 16> rtosScheduler;
};
