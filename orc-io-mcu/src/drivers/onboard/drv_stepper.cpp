#include "drv_stepper.h"

StepperDriver_t stepperDriver;
StepperDevice_t stepperDevice;

bool stepper_init(void) {
    stepperDriver.stepper = new TMC5130(PIN_STP_CS, &SPI1);
    stepperDriver.device = &stepperDevice;
    stepperDriver.ready = false;
    stepperDriver.fault = false;
    
    // Initialize device object
    stepperDevice.rpm = 0;
    stepperDevice.running = false;
    stepperDevice.enabled = false;
    strcpy(stepperDevice.unit, "rpm");
    stepperDevice.fault = false;
    stepperDevice.newMessage = false;
    stepperDevice.message[0] = '\0';
    
    // Add to object index (fixed index 26)
    objIndex[26].type = OBJ_T_STEPPER_MOTOR;
    objIndex[26].obj = &stepperDevice;
    strcpy(objIndex[26].name, "Stepper Motor");
    objIndex[26].valid = true;

    if (!stepperDriver.stepper->begin()) {
        stepperDriver.fault = true;
        stepperDriver.newMessage = true;
        strcpy(stepperDriver.message, "Stepper initialisation failed");
        
        // Propagate fault to device object
        stepperDevice.fault = true;
        stepperDevice.newMessage = true;
        strcpy(stepperDevice.message, stepperDriver.message);
        
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
            
            // Propagate fault
            stepperDevice.fault = true;
            stepperDevice.newMessage = true;
            strcpy(stepperDevice.message, stepperDriver.message);
            return false;
        }
        if (!stepperDriver.stepper->setStepsPerRev(stepperDevice.stepsPerRev)) {
            stepperDriver.fault = true;
            stepperDriver.newMessage = true;
            strcpy(stepperDriver.message, "Stepper steps per rev not set");
            
            // Propagate fault
            stepperDevice.fault = true;
            stepperDevice.newMessage = true;
            strcpy(stepperDevice.message, stepperDriver.message);
            return false;
        }
        if (!stepperDriver.stepper->invertDirection(stepperDevice.inverted)) {
            stepperDriver.fault = true;
            stepperDriver.newMessage = true;
            strcpy(stepperDriver.message, "Stepper inversion not set");
            
            // Propagate fault
            stepperDevice.fault = true;
            stepperDevice.newMessage = true;
            strcpy(stepperDevice.message, stepperDriver.message);
            return false;
        }
        if (!stepperDriver.stepper->setDirection(stepperDevice.direction)) {
            stepperDriver.fault = true;
            stepperDriver.newMessage = true;
            strcpy(stepperDriver.message, "Stepper direction not set");
            
            // Propagate fault
            stepperDevice.fault = true;
            stepperDevice.newMessage = true;
            strcpy(stepperDevice.message, stepperDriver.message);
            return false;
        }
        if (!stepperDriver.stepper->setAcceleration(stepperDevice.acceleration)) {
            stepperDriver.fault = true;
            stepperDriver.newMessage = true;
            strcpy(stepperDriver.message, "Stepper acceleration not set");
            
            // Propagate fault
            stepperDevice.fault = true;
            stepperDevice.newMessage = true;
            strcpy(stepperDevice.message, stepperDriver.message);
            return false;
        }
        if (!stepperDriver.stepper->setIhold(stepperDevice.holdCurrent)) {
            stepperDriver.fault = true;
            stepperDriver.newMessage = true;
            strcpy(stepperDriver.message, "Stepper hold current not set");
            
            // Propagate fault
            stepperDevice.fault = true;
            stepperDevice.newMessage = true;
            strcpy(stepperDevice.message, stepperDriver.message);
            return false;
        }
        if (!stepperDriver.stepper->setIrun(stepperDevice.runCurrent)) {
            stepperDriver.fault = true;
            stepperDriver.newMessage = true;
            strcpy(stepperDriver.message, "Stepper run current not set");
            
            // Propagate fault
            stepperDevice.fault = true;
            stepperDevice.newMessage = true;
            strcpy(stepperDevice.message, stepperDriver.message);
            return false;
        }
        stepperDriver.ready = true;
        
        // Check if start, stop, or RPM update is required
        if (!stepperDevice.enabled && stepperDriver.stepper->status.running) {
            // Need to stop
            if (!stepperDriver.stepper->stop()) {
                stepperDriver.fault = true;
                stepperDriver.newMessage = true;
                strcpy(stepperDriver.message, "Stepper stop failed");
                
                // Propagate fault
                stepperDevice.fault = true;
                stepperDevice.newMessage = true;
                strcpy(stepperDevice.message, stepperDriver.message);
                return false;
            }
            Serial.printf("[STEPPER] Stopped motor\n");
        } else if (stepperDevice.enabled && !stepperDriver.stepper->status.running) {
            // Need to start
            if (!stepperDriver.stepper->setRPM(stepperDevice.rpm)) {
                stepperDriver.fault = true;
                stepperDriver.newMessage = true;
                strcpy(stepperDriver.message, "Stepper RPM not set");
                
                // Propagate fault
                stepperDevice.fault = true;
                stepperDevice.newMessage = true;
                strcpy(stepperDevice.message, stepperDriver.message);
                return false;
            }
            if (!stepperDriver.stepper->run()) {
                stepperDriver.fault = true;
                stepperDriver.newMessage = true;
                strcpy(stepperDriver.message, "Stepper start failed");
                
                // Propagate fault
                stepperDevice.fault = true;
                stepperDevice.newMessage = true;
                strcpy(stepperDevice.message, stepperDriver.message);
                return false;
            }
            Serial.printf("[STEPPER] Started motor at %.1f RPM, dir=%d\n", 
                         stepperDevice.rpm, stepperDevice.direction);
        } else if (stepperDevice.enabled && stepperDriver.stepper->status.running) {
            // Motor is running - update RPM if changed
            if (!stepperDriver.stepper->setRPM(stepperDevice.rpm)) {
                stepperDriver.fault = true;
                stepperDriver.newMessage = true;
                strcpy(stepperDriver.message, "Stepper RPM update failed");
                
                // Propagate fault
                stepperDevice.fault = true;
                stepperDevice.newMessage = true;
                strcpy(stepperDevice.message, stepperDriver.message);
                return false;
            }
            Serial.printf("[STEPPER] Updated RPM to %.1f while running\n", stepperDevice.rpm);
        }
    }
    // Update status
    stepperDevice.running = stepperDriver.stepper->status.running;

    // Debug
    Serial.printf("[STEPPER] Max RPM: %.1f, Irun: %d, Ihold: %d, Acceleration: %d\n", 
                 stepperDriver.stepper->config.max_rpm, stepperDriver.stepper->config.irun, stepperDriver.stepper->config.ihold, stepperDriver.stepper->config.accelleration);

    return true;
}