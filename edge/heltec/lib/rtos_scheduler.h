#pragma once

#include <Arduino.h>
#include <functional>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// RTOS-backed task scheduler.
// - Executes callbacks on a dedicated FreeRTOS task
// - Maintains interval scheduling with wrap-around safe timings
// - API mirrors cooperative TaskScheduler for ease of adoption

template <typename State>
using RtosTaskCallback = std::function<void(State &)>;

template <typename State, size_t MaxTasks>
class RtosTaskScheduler {
public:
    RtosTaskScheduler()
        : taskCount(0), schedulerTask(nullptr), running(false), statePtr(nullptr), mutex(xSemaphoreCreateMutex()) {}

    ~RtosTaskScheduler() {
        stop();
        if (mutex) vSemaphoreDelete(mutex);
    }

    bool registerTask(const char* name, RtosTaskCallback<State> callback, uint32_t intervalMs) {
        if (running) {
            // For simplicity, do not support runtime registration after start.
            return false;
        }
        if (taskCount >= MaxTasks) return false;

        Task &t = tasks[taskCount++];
        t.name = name;
        t.callback = callback;
        t.intervalMs = intervalMs;
        t.nextRunMs = millis() + intervalMs;
        t.enabled = true;
        return true;
    }

    void setEnabled(const char* name, bool enabled) {
        xSemaphoreTake(mutex, pdMS_TO_TICKS(5));
        for (size_t i = 0; i < taskCount; i++) {
            if (tasks[i].name && name && strcmp(tasks[i].name, name) == 0) {
                tasks[i].enabled = enabled;
                break;
            }
        }
        xSemaphoreGive(mutex);
    }

    bool start(State& state, const char* taskName = "scheduler", uint32_t stackWords = 4096, unsigned int priority = tskIDLE_PRIORITY + 1) {
        if (running) return true;
        statePtr = &state;
        running = true;
        BaseType_t ok = xTaskCreate(&RtosTaskScheduler::taskTrampoline, taskName, stackWords, this, priority, &schedulerTask);
        if (ok != pdPASS) {
            running = false;
            schedulerTask = nullptr;
        }
        return ok == pdPASS;
    }

    void stop() {
        if (!running) return;
        running = false;
        TaskHandle_t handle = schedulerTask;
        schedulerTask = nullptr;
        if (handle) {
            // Let the task exit naturally on next loop
        }
    }

private:
    struct Task {
        const char* name;
        RtosTaskCallback<State> callback;
        uint32_t intervalMs;
        uint32_t nextRunMs;
        bool enabled;
    };

    static void taskTrampoline(void* arg) {
        static_cast<RtosTaskScheduler*>(arg)->runLoop();
    }

    void runLoop() {
        while (running) {
            uint32_t now = millis();
            uint32_t earliestDeltaMs = 1000; // cap sleep to re-evaluate frequently

            // Copy of task metadata under lock to minimize hold time
            xSemaphoreTake(mutex, pdMS_TO_TICKS(5));
            size_t localCount = taskCount;
            for (size_t i = 0; i < localCount; i++) {
                Task &t = tasks[i];
                if (!t.enabled) continue;
                if ((int32_t)(now - t.nextRunMs) >= 0) {
                    // Execute without holding the mutex
                    xSemaphoreGive(mutex);
                    if (statePtr) {
                        statePtr->nowMs = now;
                        t.callback(*statePtr);
                    }
                    xSemaphoreTake(mutex, pdMS_TO_TICKS(5));
                    // Recompute schedule
                    uint32_t scheduled = t.nextRunMs + t.intervalMs;
                    if ((int32_t)(now - scheduled) >= 0) {
                        t.nextRunMs = now + t.intervalMs;
                    } else {
                        t.nextRunMs = scheduled;
                    }
                }
                // Track earliest time until next run
                uint32_t delta = (int32_t)(t.nextRunMs - now) >= 0 ? (t.nextRunMs - now) : 0;
                if (delta < earliestDeltaMs) earliestDeltaMs = delta;
            }
            xSemaphoreGive(mutex);

            // Sleep until next expected task, with floor of 1 tick and ceiling for jitter control
            if (earliestDeltaMs == 0) {
                vTaskDelay(pdMS_TO_TICKS(1));
            } else {
                if (earliestDeltaMs > 100) earliestDeltaMs = 100; // responsiveness cap
                vTaskDelay(pdMS_TO_TICKS(earliestDeltaMs));
            }
        }
        vTaskDelete(nullptr);
    }

    Task tasks[MaxTasks];
    size_t taskCount;
    TaskHandle_t schedulerTask;
    volatile bool running;
    State* statePtr;
    SemaphoreHandle_t mutex;
};


