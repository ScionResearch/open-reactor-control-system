#include <Arduino.h>
#include "Scheduler.h"

TaskScheduler scheduler;
ScheduledTask* fastTask;
ScheduledTask* slowTask;
ScheduledTask* heavyTask;

// Example task functions
void fastTaskFunction() {
    // Simulate a quick task (10μs)
    delayMicroseconds(10);
}

void slowTaskFunction() {
    // Simulate a moderate task (100μs)
    delayMicroseconds(100);
}

void heavyTaskFunction() {
    // Simulate a heavy task (1000μs = 1ms)
    delayMicroseconds(1000);
}

void printCpuUsageTask() {
    // Print CPU usage report every 5 seconds
    scheduler.printCpuUsageReport();
}

void setup() {
    Serial.begin(115200);
    Serial.println("CPU Usage Monitoring Example");
    Serial.println("============================");
    
    // Add tasks with different intervals and workloads
    fastTask = scheduler.addTask(fastTaskFunction, 10, true, false);    // Every 10ms
    slowTask = scheduler.addTask(slowTaskFunction, 100, true, false);   // Every 100ms  
    heavyTask = scheduler.addTask(heavyTaskFunction, 500, true, false); // Every 500ms
    
    // Add a task to print CPU usage report every 5 seconds
    scheduler.addTask(printCpuUsageTask, 5000, true, false);
    
    Serial.println("Tasks started. CPU usage will be reported every 5 seconds.");
    Serial.println("Expected usage:");
    Serial.println("- Fast task: ~0.1% (10μs every 10ms)");
    Serial.println("- Slow task: ~0.1% (100μs every 100ms)");  
    Serial.println("- Heavy task: ~0.2% (1000μs every 500ms)");
    Serial.println("- Total: ~0.4%");
    Serial.println();
}

void loop() {
    scheduler.update();
    
    // You can also get individual task CPU usage programmatically:
    // float fastTaskCpu = fastTask->getCpuUsagePercent();
    // float totalCpu = scheduler.getTotalCpuUsagePercent();
}
