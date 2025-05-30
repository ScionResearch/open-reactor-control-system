#include <Scheduler.h>
#define LED_PIN 13

TaskScheduler scheduler;

   void blinkLED() {
       digitalWrite(LED_PIN, !digitalRead(LED_PIN));
   }

   void setup() {
       pinMode(LED_PIN, OUTPUT);
       ScheduledTask *task1 = scheduler.addTask(blinkLED, 1000, true, true);  // Blink every 1 second, high priority
   }

   void loop() {
       scheduler.update();
   }