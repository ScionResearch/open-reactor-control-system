#pragma once

#include "sys_init.h"

#include "MCP346x.h"

#define ADC_V_DIV_RATIO     5.024
#define ADC_uV_PER_LSB      314.0     //MCP346X_uV_PER_LSB * ADC_V_DIV_RATIO
#define ADC_mV_PER_LSB      0.314     //ADC_uV_PER_LSB / 1000
#define ADC_V_PER_LSB       0.000314  //ADC_uV_PER_LSB / 1000000
#define ADC_mA_PER_LSB      0.001308333   //ADC_uV_PER_LSB / 240000

struct ADCDriver_t {
    AnalogInput_t *inputObj[8];
    bool ready;
    bool fault;
    bool newMessage;
    char message[100];
    MCP346x *adc;
};

extern AnalogInput_t adcInput[8];
extern ADCDriver_t adcDriver;

bool ADC_init(void);
void ADC_update(void);