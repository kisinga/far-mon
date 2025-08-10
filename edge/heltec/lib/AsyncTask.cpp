#include "AsyncTask.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

AsyncTaskManager::AsyncTaskManager() {
}

void AsyncTaskManager::init() {
    // No specific initialization needed for FreeRTOS here, tasks are created on registration
}

void AsyncTaskManager::registerTask(const char* name, unsigned long interval, void (*callback)()) {
    TaskInfo newTask;
    newTask.name = name;
    newTask.interval = interval;
    newTask.callback = callback;
    newTask.handle = NULL; // Will be set by xTaskCreate if successful
    newTask.lastExecutionTime = millis();

    _tasks.push_back(newTask);

    // Create a FreeRTOS task for each registered task
    xTaskCreate(
        AsyncTaskManager::taskExecutor,   // Task function
        name,                             // A name just for humans
        10000,                            // Stack size (bytes)
        &_tasks.back(),                   // Parameter to pass to function
        1,                                // Task priority
        &_tasks.back().handle             // Task handle
    );
}

void AsyncTaskManager::loop() {
    // FreeRTOS tasks run independently, no need for a loop in the manager itself for execution.
    // This method could be used for other management, e.g., checking task health, but not for execution.
}

void AsyncTaskManager::taskExecutor(void* parameter) {
    TaskInfo* taskInfo = static_cast<TaskInfo*>(parameter);
    for (;;) {
        if ((millis() - taskInfo->lastExecutionTime) >= taskInfo->interval) {
            if (taskInfo->callback) {
                taskInfo->callback();
            }
            taskInfo->lastExecutionTime = millis();
        }
        vTaskDelay(10 / portTICK_PERIOD_MS); // Yield to other tasks
    }
}