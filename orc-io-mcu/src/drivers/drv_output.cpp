#include "drv_output.h"

OutputDriver_t outputDriver;
DigitalOutput_t digitalOutput[4];

void output_init(void) {
    int outputPins[4] = {PIN_OUT_1, PIN_OUT_2, PIN_OUT_3, PIN_OUT_4};
    analogWriteResolution(8);

    for (int i = 0; i < 4; i++) {
        outputDriver.outputObj[i] = &digitalOutput[i];
        outputDriver.pin[i] = outputPins[i];
        pinMode(outputDriver.pin[i], OUTPUT);
        digitalWrite(outputDriver.pin[i], LOW);
    }
}

void output_update(void) {
    for (int i = 0; i < 4; i++) {
        if (outputDriver.outputObj[i]->pwmEnabled) {
            if (outputDriver.outputObj[i]->pwmDuty > 100) outputDriver.outputObj[i]->pwmDuty = 100;
            else if (outputDriver.outputObj[i]->pwmDuty < 0) outputDriver.outputObj[i]->pwmDuty = 0;
            analogWrite(outputDriver.pin[i], static_cast<uint8_t>(outputDriver.outputObj[i]->pwmDuty * 2.55));
        } else {
            digitalWrite(outputDriver.pin[i], outputDriver.outputObj[i]->state);
        }
    }
}