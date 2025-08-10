#ifndef ASYNC_TASK_H
#define ASYNC_TASK_H

#include <Arduino.h>
#include <vector>

// Structure to hold task information
struct TaskInfo {
    const char* name;
    unsigned long interval;
    void (*callback)(); // Changed to C-style function pointer
    TaskHandle_t handle;
    unsigned long lastExecutionTime;
};

class AsyncTaskManager {
public:
    AsyncTaskManager();
    void init();
    void registerTask(const char* name, unsigned long interval, void (*callback)()); // Changed to C-style function pointer
    void loop();

private:
    std::vector<TaskInfo> _tasks;

    static void taskExecutor(void* parameter);
};

#endif // ASYNC_TASK_H