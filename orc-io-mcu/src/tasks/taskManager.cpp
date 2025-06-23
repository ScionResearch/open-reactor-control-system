#include "taskManager.h"

TaskScheduler tasks;

ScheduledTask *output_task;
ScheduledTask *gpio_task;
ScheduledTask *modbus_task;
ScheduledTask *phProbe_task;
ScheduledTask *levelProbe_task;
ScheduledTask *PARsensor_task;
ScheduledTask *printStuff_task;
ScheduledTask *RTDsensor_task;