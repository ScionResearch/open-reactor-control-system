#include "drv_dac.h"

AnalogOutput_t dacOutput[2] = {};
DACDriver_t dacDriver;

bool DAC_init(void) {
    dacDriver.dac = new MCP48FEBxx(PIN_DAC_CS, PIN_DAC_SYNC, &SPI);
    if (!dacDriver.dac->begin()) {
        dacDriver.fault = true;
        dacDriver.newMessage = true;
        sprintf(dacDriver.message, "DAC initialisation failed");
        return false;
    }

    for (int i = 0; i < 2; i++) {
        dacDriver.outputObj[i] = &dacOutput[i];
        dacDriver.outputObj[i]->value = 0;
        dacDriver.outputObj[i]->cal = &calTable[i + CAL_DAC_PTR];
        strcpy(dacDriver.outputObj[i]->unit, "mV");
        if (!dacDriver.dac->setVREF(i, VREF_EXT_BUFFERED)) {
            dacDriver.outputObj[i]->fault = true;
            dacDriver.outputObj[i]->newMessage = true;
            sprintf(dacDriver.outputObj[i]->message, "Failed to set ch %d VREF", i);
        } else if (!dacDriver.dac->setGain(i, GAIN_1X)) {
            dacDriver.outputObj[i]->fault = true;
            dacDriver.outputObj[i]->newMessage = true;
            sprintf(dacDriver.outputObj[i]->message, "Failed to set ch %d gain", i);
        } else if (!dacDriver.dac->setPD(i, PD_NORMAL) && !dacDriver.dac->setPD(i, PD_NORMAL)) {
            dacDriver.outputObj[i]->fault = true;
            dacDriver.outputObj[i]->newMessage = true;
            sprintf(dacDriver.outputObj[i]->message, "Failed to set ch %d power mode", i);
        } else if (!dacDriver.dac->writeDAC(i, 0)) {
            dacDriver.outputObj[i]->fault = true;
            dacDriver.outputObj[i]->newMessage = true;
            sprintf(dacDriver.outputObj[i]->message, "Failed to write ch %d DAC output", i);
        } else {
            dacDriver.outputObj[i]->enabled = true;
            dacDriver.outputObj[i]->fault = false;
            dacDriver.outputObj[i]->newMessage = false;
        }
    }
    if (!dacDriver.dac->saveRegistersToEEPROM()) {
        dacDriver.fault = true;
        dacDriver.newMessage = true;
        sprintf(dacDriver.message, "Failed to save DAC registers to EEPROM");
    }
    if (dacDriver.fault || dacDriver.outputObj[0]->fault || dacDriver.outputObj[1]->fault) {
        return false;
    }
    return true;
}

bool DAC_writeOutputs(void) {
    float dacVal[2];
    bool fault_occured = false;
    for (int i = 0; i < 2; i++) {
        if (dacDriver.outputObj[i]->enabled) {
            if (dacDriver.outputObj[i]->value > 10240) dacDriver.outputObj[i]->value = 10240;
            else if (dacDriver.outputObj[i]->value < 0) dacDriver.outputObj[i]->value = 0;
            dacVal[i] = dacDriver.outputObj[i]->value/mV_PER_LSB;
            dacVal[i] *= dacDriver.outputObj[i]->cal->scale;
            dacVal[i] += dacDriver.outputObj[i]->cal->offset;
            if (!dacDriver.dac->writeDAC(i, (uint16_t)dacVal[i])) {
                fault_occured = true;
                dacDriver.outputObj[i]->fault = true;
                dacDriver.outputObj[i]->newMessage = true;
                dacDriver.outputObj[i]->enabled = false;
                sprintf(dacDriver.outputObj[i]->message, "Failed to write ch %d DAC output", i);
            }
        }
    }
    if (fault_occured) return false;
    return true;
}