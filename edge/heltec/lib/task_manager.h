// Task Manager - Simplified task management with common patterns
// Provides clean interfaces for task registration and execution

#pragma once

#include <functional>
#include <vector>
#include <string>
#include "scheduler.h"

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
    explicit TaskManager(size_t maxTasks = 16) : maxTasks(maxTasks) {
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
        // Also register with the underlying cooperative scheduler so it actually runs
        scheduler.registerTask(tasks.back().name.c_str(), tasks.back().callback, intervalMs);
        return true;
    }

    // Update task execution
    void update(CommonAppState& state) {
        state.nowMs = millis();
        scheduler.tick(state);
    }

    // Enable/disable tasks
    void setTaskEnabled(const std::string& name, bool enabled) {
        scheduler.setEnabled(name.c_str(), enabled);
    }

private:
    size_t maxTasks;
    std::vector<TaskDefinition> tasks;
    TaskScheduler<CommonAppState, 16> scheduler;
};
