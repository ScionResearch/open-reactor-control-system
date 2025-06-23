#include "drv_output.h"

OutputDriver_t outputDriver;
DigitalOutput_t digitalOutput[4];
DigitalOutput_t heaterOutput[1];

void output_init(void) {
    int outputPins[4] = {PIN_OUT_1, PIN_OUT_2, PIN_OUT_3, PIN_OUT_4};
    analogWriteResolution(8);

    // Setup open-drain outputs
    for (int i = 0; i < 4; i++) {
        outputDriver.outputObj[i] = &digitalOutput[i];
        outputDriver.pin[i] = outputPins[i];
        pinMode(outputDriver.pin[i], OUTPUT);
        digitalWrite(outputDriver.pin[i], LOW);
    }

    // Setup heater high current output (slow PWM)
    outputDriver.outputObj[4] = &heaterOutput[0];
    outputDriver.pin[4] = PIN_HEAT_OUT;

    // Initialise the PWM function using the standard core implementation
    analogWrite(PIN_HEAT_OUT, 0);

    // Enable TCC0 clock
    GCLK->PCHCTRL[TCC0_GCLK_ID].reg = GCLK_PCHCTRL_CHEN | GCLK_PCHCTRL_GEN_GCLK0;
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
        if (outputDriver.outputObj[i]->pwmEnabled) {
            if (outputDriver.outputObj[i]->pwmDuty > 100) outputDriver.outputObj[i]->pwmDuty = 100;
            else if (outputDriver.outputObj[i]->pwmDuty < 0) outputDriver.outputObj[i]->pwmDuty = 0;
            analogWrite(outputDriver.pin[i], static_cast<uint8_t>(outputDriver.outputObj[i]->pwmDuty * 2.55));
        } else {
            digitalWrite(outputDriver.pin[i], outputDriver.outputObj[i]->state);
        }
    }

    // Update heater output - 1Hz PWM only, custom implementation
    static bool heaterEnabled = false;
    if (heaterOutput[0].pwmEnabled) {
        if (!heaterEnabled) {
            // Enable heater output
            TCC0->CTRLA.bit.ENABLE = 1;
            while (TCC0->SYNCBUSY.bit.ENABLE);
            heaterEnabled = true;
        }
        if (heaterOutput[0].pwmDuty > 100) heaterOutput[0].pwmDuty = 100;
        else if (heaterOutput[0].pwmDuty < 0) heaterOutput[0].pwmDuty = 0;
        static uint32_t prev_duty = 0;
        uint32_t duty = heaterOutput[0].pwmDuty * HEATER_PWM_PERIOD / 100;
        if (duty != prev_duty) {
            TCC0->CC[4].reg = duty;
            while (TCC0->SYNCBUSY.bit.CC4);
            prev_duty = duty;
        }
    } else if (heaterEnabled) {
        // Disable heater output
        TCC0->CTRLA.bit.ENABLE = 0;
        while (TCC0->SYNCBUSY.bit.ENABLE);
        heaterEnabled = false;
    }
}