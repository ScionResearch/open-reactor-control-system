#pragma once

#include "sys_init.h"

#include "DRV8235.h"

struct MotorDriver_t {
    DRV8235 *motor;
    MotorDevice_t *device;
    bool ready;
    bool fault;
    bool newMessage;
    char message[100];
};

extern MotorDriver_t motorDriver[4];
extern MotorDevice_t motorDevice[4];

bool motor_init(void);
void motor_update(void);
bool motor_stop(uint8_t motor);
bool motor_run(uint8_t motor);
bool motor_run(uint8_t motor, uint8_t speed, bool reverse);