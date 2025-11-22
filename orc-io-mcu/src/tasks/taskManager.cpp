#include "taskManager.h"

TaskScheduler tasks;

ScheduledTask *analog_input_task;
ScheduledTask *analog_output_task;
ScheduledTask *output_task;
ScheduledTask *gpio_task;
ScheduledTask *modbus_task;
ScheduledTask *ipc_task;
ScheduledTask *phProbe_task;
ScheduledTask *mfc_task;
ScheduledTask *pwrSensor_task;
ScheduledTask *stepper_task;
ScheduledTask *motor_task;
ScheduledTask *printStuff_task;
ScheduledTask *RTDsensor_task;
ScheduledTask *SchedulerAlive_task;

ScheduledTask *DEBUG_TASK;