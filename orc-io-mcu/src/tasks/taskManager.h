#pragma once

#include "sys_init.h"

#include "Scheduler.h"

extern TaskScheduler tasks;

extern ScheduledTask *output_task;
extern ScheduledTask *gpio_task;
extern ScheduledTask *modbus_task;
extern ScheduledTask *phProbe_task;
extern ScheduledTask *levelProbe_task;
extern ScheduledTask *PARsensor_task;
extern ScheduledTask *printStuff_task;