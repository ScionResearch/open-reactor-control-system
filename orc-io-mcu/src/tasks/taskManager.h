#pragma once

#include "Scheduler.h"

extern TaskScheduler tasks;

extern ScheduledTask *analog_input_task;
extern ScheduledTask *analog_output_task;
extern ScheduledTask *output_task;
extern ScheduledTask *gpio_task;
extern ScheduledTask *modbus_task;
extern ScheduledTask *ipc_task;
extern ScheduledTask *phProbe_task;
extern ScheduledTask *mfc_task;
extern ScheduledTask *levelProbe_task;
extern ScheduledTask *PARsensor_task;
extern ScheduledTask *printStuff_task;
extern ScheduledTask *RTDsensor_task;
extern ScheduledTask *SchedulerAlive_task;

extern ScheduledTask *TEST_TASK;
