#pragma once

#include "sys_init.h"

#define DAC_RANGE         4095
#define VREF_mV           2048
#define OP_AMP_GAIN       5
#define VOUT_MAX_mV       10240
#define mV_PER_LSB        2.50  // DAC Range = 4095, Vref = 2.048V, Op-Amp gain = 5, Vout max = 10.24V, mV/LSB = Vmax/DAC_range = 10.24/4095 = 2.50mV/LSB

// Driver struct - contains interface parameters and sensor object
struct DACDriver_t {
    AnalogOutput_t *outputObj[2];
    bool ready;
    bool fault;
    bool newMessage;
    char message[100];
    MCP48FEBxx *dac;
};

extern AnalogOutput_t dacOutput[2];
extern DACDriver_t dacDriver;

bool DAC_init(void);
bool DAC_writeOutputs(void);