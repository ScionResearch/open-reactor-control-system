#include "drv_output.h"

OutputDriver_t outputDriver;
DigitalOutput_t digitalOutput[4];
DigitalOutput_t heaterOutput[1];

void output_init(void) {
    int outputPins[4] = {PIN_OUT_1, PIN_OUT_2, PIN_OUT_3, PIN_OUT_4};
    analogWriteResolution(8);

    // Setup open-drain outputs (indices 21-24)
    for (int i = 0; i < 4; i++) {
        outputDriver.outputObj[i] = &digitalOutput[i];
        outputDriver.pin[i] = outputPins[i];
        pinMode(outputDriver.pin[i], OUTPUT);
        digitalWrite(outputDriver.pin[i], LOW);
        
        // Add to object index (fixed indices 21-24)
        objIndex[21 + i].type = OBJ_T_DIGITAL_OUTPUT;
        objIndex[21 + i].obj = &digitalOutput[i];
        sprintf(objIndex[21 + i].name, "Digital Output %d", i + 1);
        objIndex[21 + i].valid = true;
    }

    // Setup heater high current output (slow PWM, index 25)
    outputDriver.outputObj[4] = &heaterOutput[0];
    outputDriver.pin[4] = PIN_HEAT_OUT;
    
    // Add to object index (fixed index 25)
    objIndex[25].type = OBJ_T_DIGITAL_OUTPUT;
    objIndex[25].obj = &heaterOutput[0];
    strcpy(objIndex[25].name, "Heater Output");
    objIndex[25].valid = true;

    // Initialise the PWM function using the standard core implementation
    analogWrite(PIN_HEAT_OUT, 0);

    // Enable TCC0 clock
    while (!(GCLK->PCHCTRL[TCC0_GCLK_ID].reg & GCLK_PCHCTRL_CHEN));

    // Disable TCC0
    TCC0->CTRLA.bit.ENABLE = 0;
    while (TCC0->SYNCBUSY.bit.ENABLE);

    // Reset TCC0
    TCC0->CTRLA.bit.SWRST = 1;
    while (TCC0->SYNCBUSY.bit.SWRST || TCC0->CTRLA.bit.SWRST);

    // Use prescaler of 1024
    TCC0->CTRLA.reg = TCC_CTRLA_PRESCALER_DIV1024 | TCC_CTRLA_PRESCSYNC_GCLK;

    // Set waveform generation to normal PWM (match on CCx)
    TCC0->WAVE.reg = TCC_WAVE_WAVEGEN_NPWM;
    while (TCC0->SYNCBUSY.bit.WAVE);

    TCC0->PER.reg = HEATER_PWM_PERIOD;
    while (TCC0->SYNCBUSY.bit.PER);

    // Set 0% duty cycle
    TCC0->CC[4].reg = 0;
    while (TCC0->SYNCBUSY.bit.CC4);

    // Enable TCC0
    /*TCC0->CTRLA.bit.ENABLE = 1;
    while (TCC0->SYNCBUSY.bit.ENABLE);*/
}

void output_update(void) {
    // Update open-drain outputs
    for (int i = 0; i < 4; i++) {
        // Local copy of output states so we don't waste time on long analogWrite calls
        static uint8_t pwmDuty[4] = {0, 0, 0, 0};
        static bool state[4] = {false, false, false, false};

        if (outputDriver.outputObj[i]->pwmEnabled && pwmDuty[i] != outputDriver.outputObj[i]->pwmDuty) {
            if (outputDriver.outputObj[i]->pwmDuty > 100) outputDriver.outputObj[i]->pwmDuty = 100;
            else if (outputDriver.outputObj[i]->pwmDuty < 0) outputDriver.outputObj[i]->pwmDuty = 0;
            pwmDuty[i] = outputDriver.outputObj[i]->pwmDuty;
            uint32_t ts = micros();
            analogWrite(outputDriver.pin[i], static_cast<uint8_t>(outputDriver.outputObj[i]->pwmDuty * 2.55));
            uint32_t te = micros();
            Serial.printf("Output %d: Analog write took %d us\n", i, te - ts);
        } else if (state[i] != outputDriver.outputObj[i]->state) {
            digitalWrite(outputDriver.pin[i], outputDriver.outputObj[i]->state);
            state[i] = outputDriver.outputObj[i]->state;
        }
    }

    // Update heater output - 1Hz PWM only, custom implementation
    static bool heaterEnabled = false;
    static uint8_t pwmDuty = 0;
    if (heaterOutput[0].pwmEnabled) {
        if (!heaterEnabled) {
            // Enable heater output
            TCC0->CTRLA.bit.ENABLE = 1;
            while (TCC0->SYNCBUSY.bit.ENABLE);
            heaterEnabled = true;
        }
        if (pwmDuty != heaterOutput[0].pwmDuty) {
            if (heaterOutput[0].pwmDuty > 100) heaterOutput[0].pwmDuty = 100;
            else if (heaterOutput[0].pwmDuty < 0) heaterOutput[0].pwmDuty = 0;
            static uint32_t prev_duty = 0;
            uint32_t duty = heaterOutput[0].pwmDuty * HEATER_PWM_PERIOD / 100;
            if (duty != prev_duty) {
                TCC0->CC[4].reg = duty;
                while (TCC0->SYNCBUSY.bit.CC4);
                prev_duty = duty;
            }
            pwmDuty = heaterOutput[0].pwmDuty;
        }
    } else if (heaterEnabled) {
        // Disable heater output
        TCC0->CTRLA.bit.ENABLE = 0;
        while (TCC0->SYNCBUSY.bit.ENABLE);
        heaterEnabled = false;
    }
}