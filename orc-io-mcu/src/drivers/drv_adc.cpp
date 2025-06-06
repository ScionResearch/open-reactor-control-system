#include "drv_adc.h"

AnalogInput_t adcInput[8];
ADCDriver_t adcDriver;

bool ADC_init(void) {
    adcDriver.adc = new MCP346x(PIN_ADC_CS, PIN_ADC_IRQ, &SPI);
    for (int i = 0; i < 8; i++) {
        // Pre-set inpud data objects
        adcDriver.inputObj[i] = &adcInput[i];
        adcDriver.inputObj[i]->value = 0;
        adcDriver.inputObj[i]->cal = &calTable[i + CAL_ADC_PTR];
        strcpy(adcDriver.inputObj[i]->unit, "mV");

        // Add to object index
        objIndex[numObjects].type = OBJ_T_ANALOG_INPUT;
        objIndex[numObjects].obj = &adcDriver.inputObj[i];
        strcpy(objIndex[numObjects].name, "Analogue Input ");
        strcat(objIndex[numObjects].name, String(i+1).c_str());
        objIndex[numObjects].valid = true;
        numObjects++;
    }
    if (!adcDriver.adc->begin()) {
        adcDriver.fault = true;
        adcDriver.ready = false;
        adcDriver.newMessage = true;
        strcpy(adcDriver.message, "ADC initialisation failed");
        return false;
    }
    if (!adcDriver.adc->start_continuous_adc(MCP346X_SCAN_ALL_CH)) {
        adcDriver.fault = true;
        adcDriver.ready = false;
        adcDriver.newMessage = true;
        strcpy(adcDriver.message, "ADC failed to start conversion");
        return false;
    }
    adcDriver.fault = false;
    adcDriver.ready = true;
    adcDriver.newMessage = true;
    strcpy(adcDriver.message, "ADC initialisation successful");
    return true;
}

bool ADC_readInputs(void) {
    if (!adcDriver.adc->descriptor.new_data) {
        adcDriver.ready = false;
        adcDriver.newMessage = true;
        sprintf(adcDriver.message, "ADC not ready");
        return false;  // No new data available from ADC, so return false and wait for next time
    }
    adcDriver.adc->descriptor.new_data = 0;
    adcDriver.ready = true;
    for (int i = 0; i < 8; i++) {
        // Scale and offset raw value so we don't get issues if units change
        float result = adcDriver.adc->descriptor.results[i] * adcDriver.inputObj[i]->cal->scale + adcDriver.inputObj[i]->cal->offset;
        // Check for the unit and convert accordingly
        if (strcmp(adcDriver.inputObj[i]->unit, "mV") == 0) {
            adcDriver.inputObj[i]->value = result * ADC_mV_PER_LSB;
        } else if (strcmp(adcDriver.inputObj[i]->unit, "mA") == 0) {
            adcDriver.inputObj[i]->value = result * ADC_mA_PER_LSB;
        } else if (strcmp(adcDriver.inputObj[i]->unit, "V") == 0) {
            adcDriver.inputObj[i]->value = result * ADC_V_PER_LSB;
        } else if (strcmp(adcDriver.inputObj[i]->unit, "µV") == 0) {
            adcDriver.inputObj[i]->value = result * ADC_uV_PER_LSB;
        } else {
            adcDriver.inputObj[i]->value = result * ADC_mV_PER_LSB;   // Default to mV
        }
        Serial.printf("ADC channel %d raw: %i, calculated: %0.3f%s\n", i, adcDriver.adc->descriptor.results[i], adcDriver.inputObj[i]->value, adcDriver.inputObj[i]->unit);
    }
    return true;
}