#include "drv_bdc_motor.h"

MotorDriver_t motorDriver[4];
MotorDevice_t motorDevice[4];

int currentFBpins[4] = { PIN_MOT_I_FB_1, PIN_MOT_I_FB_2, PIN_MOT_I_FB_3, PIN_MOT_I_FB_4 };
int faultIRQpins[4] = { PIN_MOT_IRQ_1, PIN_MOT_IRQ_2, PIN_MOT_IRQ_3, PIN_MOT_IRQ_4 };

bool motor_init(void) {
    for (int i = 0; i < 4; i++) {
        motorDriver[i].motor = new DRV8235(DRV8325_I2C_BASE_ADDR + i, &Wire, faultIRQpins[i], currentFBpins[i]);
        motorDriver[i].device = &motorDevice[i];
        motorDriver[i].ready = false;
        motorDriver[i].fault = false;
        motorDriver[i].newMessage = false;

        // Initialize device object
        motorDevice[i].power = 0;
        motorDevice[i].running = false;
        motorDevice[i].enabled = false;
        strcpy(motorDevice[i].unit, "%");
        motorDevice[i].fault = false;
        motorDevice[i].newMessage = false;
        motorDevice[i].message[0] = '\0';

        // Add to object index (fixed indices 27-30)
        objIndex[27 + i].type = OBJ_T_BDC_MOTOR;
        objIndex[27 + i].obj = &motorDevice[i];
        sprintf(objIndex[27 + i].name, "DC Motor %d", i + 1);
        objIndex[27 + i].valid = true;

        if (!motorDriver[i].motor->begin()) {
            motorDriver[i].fault = true;
            motorDriver[i].ready = false;
            motorDriver[i].newMessage = true;
            strcpy(motorDriver[i].message, "Motor initialisation failed");
            
            // Propagate fault to device object
            motorDevice[i].fault = true;
            motorDevice[i].newMessage = true;
            strcpy(motorDevice[i].message, motorDriver[i].message);
            
            return false;
        }
        motorDriver[i].fault = false;
        motorDriver[i].ready = true;
    }
    return true;
}

void motor_update(void) {
    for (int i = 0; i < 4; i++) {
        motorDriver[i].motor->manage();

        // Update current value
        motorDriver[i].device->runCurrent = motorDriver[i].motor->motorCurrent();

        // Check fault status and propagate to device
        if (motorDriver[i].motor->faultActive) {
            motorDriver[i].fault = true;
            motorDriver[i].newMessage = true;
            
            if (motorDriver[i].motor->powerOnReset) strcpy(motorDriver[i].message, "Motor driver restarted after power failed");
            else if (motorDriver[i].motor->overTemperature) strcpy(motorDriver[i].message, "Motor driver high temperature fault");
            else if (motorDriver[i].motor->overVoltage) strcpy(motorDriver[i].message, "Motor driver over voltage fault");
            else if (motorDriver[i].motor->overCurrent) strcpy(motorDriver[i].message, "Motor driver over current fault");
            else if (motorDriver[i].motor->stall) strcpy(motorDriver[i].message, "Motor stall detected");
            else if (motorDriver[i].motor->fault) strcpy(motorDriver[i].message, "Motor driver fault");
            else strcpy(motorDriver[i].message, "Unknown motor driver fault");
            
            // Propagate fault to device object
            motorDevice[i].fault = true;
            motorDevice[i].newMessage = true;
            strcpy(motorDevice[i].message, motorDriver[i].message);
            
            motorDriver[i].motor->faultActive = false;
        } else {
            motorDriver[i].fault = false;
            motorDriver[i].newMessage = false;
            motorDevice[i].fault = false;
            motorDevice[i].newMessage = false;
        }
    }
}

bool motor_stop(uint8_t motor) {
    if (!motorDriver[motor].motor->stop()) {
        strcpy(motorDriver[motor].message, "Failed to stop motor");
        motorDriver[motor].newMessage = true;
        motorDriver[motor].fault = true;
        return false;
    }
    motorDriver[motor].device->running = false;
    motorDriver[motor].device->runCurrent = 0;
    return true;
}

bool motor_run(uint8_t motor) {
    if (motor >= 4) return false;
    if (!motorDriver[motor].ready) {
        strcpy(motorDriver[motor].message, "Motor driver not ready");
        motorDriver[motor].newMessage = true;
        return false;
    }
    if (!motorDriver[motor].device->enabled) {
        strcpy(motorDriver[motor].message, "Motor driver not enabled");
        motorDriver[motor].newMessage = true;
        return false;
    }
    if (!motorDriver[motor].motor->run()) {
        strcpy(motorDriver[motor].message, "Failed to run motor");
        motorDriver[motor].newMessage = true;
        motorDriver[motor].fault = true;
        return false;
    }
    motorDriver[motor].device->running = true;
    return true;
}

bool motor_run(uint8_t motor, uint8_t power, bool reverse) {
    if (motor >= 4) return false;
    motorDriver[motor].device->power = power;
    motorDriver[motor].device->direction = reverse;
    /*return  motorDriver[motor].motor->setSpeed(power) && 
            motorDriver[motor].motor->direction(reverse ^ motorDriver[motor].device->inverted) && 
            motor_run(motor);*/
    if (!motorDriver[motor].motor->setSpeed(power)) {
        strcpy(motorDriver[motor].message, "Failed to set speed");
        motorDriver[motor].newMessage = true;
        motorDriver[motor].fault = true;
        return false;
    }
    if (!motorDriver[motor].motor->direction(reverse ^ motorDriver[motor].device->inverted)) {
        strcpy(motorDriver[motor].message, "Failed to set direction");
        motorDriver[motor].newMessage = true;
        motorDriver[motor].fault = true;
        return false;
    }
    return motor_run(motor);
}