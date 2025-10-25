#include "drv_pwr_sensor.h"

// Create unified energy sensor objects (1 per physical INA260)
EnergySensor_t pwr_energy[2];

PowerSensorDriver_t pwr_interface[2];

int irqPins[] = { PIN_P_MAIN_IRQ, PIN_P_HEAT_IRQ };

bool pwrSensor_init(void) {
    const char* sensorNames[] = {"Main", "Heater"};
    
    for (int i = 0; i < 2; i++) {
        pwr_interface[i].sensor = new INA260(INA260_BASE_ADDRESS + i, &Wire, irqPins[i]);
        pwr_interface[i].updateInterval = 1.0;

        // Initialize energy sensor object (consolidated voltage, current, power)
        pwr_energy[i].voltage = 0.0f;
        pwr_energy[i].current = 0.0f;
        pwr_energy[i].power = 0.0f;
        strcpy(pwr_energy[i].unit, "V");  // Primary unit is voltage
        pwr_energy[i].fault = false;
        pwr_energy[i].newMessage = false;
        pwr_energy[i].message[0] = '\0';
        
        // Add to object index (fixed indices 31-32)
        int objIndex_idx = 31 + i;
        
        objIndex[objIndex_idx].type = OBJ_T_ENERGY_SENSOR;
        objIndex[objIndex_idx].obj = &pwr_energy[i];
        sprintf(objIndex[objIndex_idx].name, "%s Power Monitor", sensorNames[i]);
        objIndex[objIndex_idx].valid = true;

        if (!pwr_interface[i].sensor->begin()) {
            // Set fault on energy sensor object
            pwr_energy[i].fault = true;
            pwr_energy[i].newMessage = true;
            sprintf(pwr_energy[i].message, "%s power sensor init failed", sensorNames[i]);
            return false;
        }

        if (!pwr_interface[i].sensor->setAverage(INA260_AVERAGE::INA260_AVERAGE_1024)) return false;
        if (!pwr_interface[i].sensor->setVoltageConversionTime(INA260_V_CONV_TIME::INA260_VBUSCT_1100US)) return false;
        if (!pwr_interface[i].sensor->setCurrentConversionTime(INA260_I_CONV_TIME::INA260_ISHCT_1100US)) return false;
    }
    return true;
}

bool pwrSensor_update(void) {
    for (int i = 0; i < 2; i++) {
        // Read all three values atomically into single object
        pwr_energy[i].voltage = pwr_interface[i].sensor->volts();
        pwr_energy[i].current = pwr_interface[i].sensor->amps();
        pwr_energy[i].power = pwr_interface[i].sensor->watts();
    }
    return true;
}