#include "drv_stepper.h"

StepperDriver_t stepperDriver;
StepperDevice_t stepperDevice;

bool stepper_init(void) {
    stepperDriver.stepper = new TMC5130(PIN_STP_CS, &SPI1);
    stepperDriver.device = &stepperDevice;
    stepperDriver.ready = false;
    stepperDriver.fault = false;

    if (!stepperDriver.stepper->begin()) {
        stepperDriver.fault = true;
        stepperDriver.newMessage = true;
        strcpy(stepperDriver.message, "Stepper initialisation failed");
        return false;
    }
    stepperDriver.stepper->setRPM(0.0);
    stepperDriver.stepper->stop();

    return true;
}

bool stepper_update(bool setParams) {
    if (setParams) {
        // Update stepper parameters
        if (!stepperDriver.stepper->setMaxRPM(stepperDevice.maxRPM)) {
            stepperDriver.fault = true;
            stepperDriver.newMessage = true;
            strcpy(stepperDriver.message, "Stepper max RPM not set");
            return false;
        }
        if (!stepperDriver.stepper->setStepsPerRev(stepperDevice.stepsPerRev)) {
            stepperDriver.fault = true;
            stepperDriver.newMessage = true;
            strcpy(stepperDriver.message, "Stepper steps per rev not set");
            return false;
        }
        if (!stepperDriver.stepper->invertDirection(stepperDevice.inverted)) {
            stepperDriver.fault = true;
            stepperDriver.newMessage = true;
            strcpy(stepperDriver.message, "Stepper inversion not set");
            return false;
        }
        if (!stepperDriver.stepper->setDirection(stepperDevice.direction)) {
            stepperDriver.fault = true;
            stepperDriver.newMessage = true;
            strcpy(stepperDriver.message, "Stepper direction not set");
            return false;
        }
        if (!stepperDriver.stepper->setAcceleration(stepperDevice.acceleration)) {
            stepperDriver.fault = true;
            stepperDriver.newMessage = true;
            strcpy(stepperDriver.message, "Stepper acceleration not set");
            return false;
        }
        if (!stepperDriver.stepper->setIhold(stepperDevice.holdCurrent)) {
            stepperDriver.fault = true;
            stepperDriver.newMessage = true;
            strcpy(stepperDriver.message, "Stepper hold current not set");
            return false;
        }
        if (!stepperDriver.stepper->setIrun(stepperDevice.runCurrent)) {
            stepperDriver.fault = true;
            stepperDriver.newMessage = true;
            strcpy(stepperDriver.message, "Stepper run current not set");
            return false;
        }
        stepperDriver.ready = true;
        
        // Check if start or stop is required
        if (!stepperDevice.enabled && stepperDriver.stepper->status.running) {
            if (!stepperDriver.stepper->stop()) {
                stepperDriver.fault = true;
                stepperDriver.newMessage = true;
                strcpy(stepperDriver.message, "Stepper stop failed");
                return false;
            }
        } else if (stepperDevice.enabled && !stepperDriver.stepper->status.running) {
            if (!stepperDriver.stepper->setRPM(stepperDevice.rpm)) {
                stepperDriver.fault = true;
                stepperDriver.newMessage = true;
                strcpy(stepperDriver.message, "Stepper RPM not set");
                return false;
            }
            if (!stepperDriver.stepper->run()) {
                stepperDriver.fault = true;
                stepperDriver.newMessage = true;
                strcpy(stepperDriver.message, "Stepper start failed");
                return false;
            }
        }
    }
    // Update status
    stepperDevice.running = stepperDriver.stepper->status.running;

    return true;
}