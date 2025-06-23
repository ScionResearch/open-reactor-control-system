#include <Scheduler.h>
#define LED_PIN 13

TaskScheduler scheduler;
ScheduledTask *blinkTask;

void blinkLED() {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
}

void printStats() {
    if (blinkTask) {
        Serial.printf("Blink task µs last: %d, min: %d, max: %d, avg: %0.2f\n",
                        blinkTask->getLastExecTime(),
                        blinkTask->getMinExecTime(),
                        blinkTask->getMaxExecTime(),
                        blinkTask->getAverageExecTime());
    } else {
        Serial.println("Blink task does not exist.");
    }
}

void setup() {
    pinMode(LED_PIN, OUTPUT);
    blinkTask = scheduler.addTask(blinkLED, 500, true, true);   // Toggle LED every 500 ms
    scheduler.addTask(printStats, 5000, true, false);           // Print scheduler stats every 5 seconds
}

void loop() {
    scheduler.update();     // Call update() frequently to ensure tasks are executed in expected intervals
}