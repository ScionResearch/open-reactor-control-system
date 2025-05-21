#include "drv_pwr_sensor.h"

PowerSensor_t pwr_sensor[2];
PowerSensorDriver_t pwr_interface[2];

int irqPins[] = { PIN_P_MAIN_IRQ, PIN_P_HEAT_IRQ };

bool pwrSensor_init(void) {
    for (int i = 0; i < 2; i++) {
        pwr_interface[i].powerObj = &pwr_sensor[i];
        pwr_interface[i].sensor = new INA260(INA260_BASE_ADDRESS + i, &Wire, irqPins[i]);
        pwr_interface[i].updateInterval = 1.0;

        if (!pwr_interface[i].sensor->begin()) return false;

        pwr_interface[i].powerObj->voltage = 0.0f;
        pwr_interface[i].powerObj->current = 0.0f;
        pwr_interface[i].powerObj->power = 0.0f;

        if (!pwr_interface[i].sensor->setAverage(INA260_AVERAGE::INA260_AVERAGE_1024)) return false;
        if (!pwr_interface[i].sensor->setVoltageConversionTime(INA260_V_CONV_TIME::INA260_VBUSCT_1100US)) return false;
        if (!pwr_interface[i].sensor->setCurrentConversionTime(INA260_I_CONV_TIME::INA260_ISHCT_1100US)) return false;
    }
    return true;
}

bool pwrSensor_update(void) {
    for (int i = 0; i < 2; i++) {
        pwr_interface[i].powerObj->voltage = pwr_interface[i].sensor->volts();
        pwr_interface[i].powerObj->current = pwr_interface[i].sensor->amps();
        pwr_interface[i].powerObj->power = pwr_interface[i].sensor->watts();
    }
    return true;
}