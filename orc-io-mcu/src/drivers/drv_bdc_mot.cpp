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

        if (!motorDriver[i].motor->begin()) {
            motorDriver[i].fault = true;
            motorDriver[i].ready = false;
            motorDriver[i].newMessage = true;
            strcpy(motorDriver[i].message, "Motor initialisation failed");
            return false;
        }
        motorDriver[i].fault = false;
        motorDriver[i].ready = true;
    }
    return true;
}

bool motor_update(void) {
    bool fault_occured = false;
    for (int i = 0; i < 4; i++) {
        motorDriver[i].motor->manage();

        // Update current value
        motorDriver[i].device->runCurrent = motorDriver[i].motor->motorCurrent();

        // Check fault status
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
            motorDriver[i].motor->faultActive = false;
            fault_occured = true;
        } else {
            motorDriver[i].fault = false;
            motorDriver[i].newMessage = false;
        }
    }
    return !fault_occured;
}

bool motor_stop(uint8_t motor) {
    if (!motorDriver[motor].motor->stop()) return false;
    motorDriver[motor].device->running = false;
    motorDriver[motor].device->runCurrent = 0;
    return true;
}

bool motor_run(uint8_t motor) {
    if (motor >= 4) return false;
    if (!motorDriver[motor].ready) {
        Serial.println("Motor driver not ready");
        return false;
    }
    if (!motorDriver[motor].device->enabled) {
        Serial.println("Motor driver not enabled");
        return false;
    }
    if (!motorDriver[motor].motor->run()) {
        Serial.println("Failed to run motor");
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
        Serial.println("Failed to set speed");
        return false;
    }
    if (!motorDriver[motor].motor->direction(reverse ^ motorDriver[motor].device->inverted)) {
        Serial.println("Failed to set direction");
        return false;
    }
    return motor_run(motor);
}