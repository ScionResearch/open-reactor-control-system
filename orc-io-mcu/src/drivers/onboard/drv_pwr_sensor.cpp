#include "drv_pwr_sensor.h"

// Create 3 separate sensor objects per physical sensor
VoltageSensor_t pwr_voltage[2];
CurrentSensor_t pwr_current[2];
PowerSensor_t pwr_power[2];

PowerSensorDriver_t pwr_interface[2];

int irqPins[] = { PIN_P_MAIN_IRQ, PIN_P_HEAT_IRQ };

bool pwrSensor_init(void) {
    const char* sensorNames[] = {"Main", "Heater"};
    
    for (int i = 0; i < 2; i++) {
        pwr_interface[i].sensor = new INA260(INA260_BASE_ADDRESS + i, &Wire, irqPins[i]);
        pwr_interface[i].updateInterval = 1.0;

        // Initialize voltage sensor object
        pwr_voltage[i].voltage = 0.0f;
        strcpy(pwr_voltage[i].unit, "V");
        pwr_voltage[i].fault = false;
        pwr_voltage[i].newMessage = false;
        pwr_voltage[i].message[0] = '\0';
        
        // Initialize current sensor object
        pwr_current[i].current = 0.0f;
        strcpy(pwr_current[i].unit, "A");
        pwr_current[i].fault = false;
        pwr_current[i].newMessage = false;
        pwr_current[i].message[0] = '\0';
        
        // Initialize power sensor object
        pwr_power[i].power = 0.0f;
        strcpy(pwr_power[i].unit, "W");
        pwr_power[i].fault = false;
        pwr_power[i].newMessage = false;
        pwr_power[i].message[0] = '\0';
        
        // Add to object index (fixed indices 31-36)
        int baseIndex = 31 + (i * 3);
        
        // Voltage sensor
        objIndex[baseIndex + 0].type = OBJ_T_VOLTAGE_SENSOR;
        objIndex[baseIndex + 0].obj = &pwr_voltage[i];
        sprintf(objIndex[baseIndex + 0].name, "%s Power Voltage", sensorNames[i]);
        objIndex[baseIndex + 0].valid = true;
        
        // Current sensor
        objIndex[baseIndex + 1].type = OBJ_T_CURRENT_SENSOR;
        objIndex[baseIndex + 1].obj = &pwr_current[i];
        sprintf(objIndex[baseIndex + 1].name, "%s Power Current", sensorNames[i]);
        objIndex[baseIndex + 1].valid = true;
        
        // Power sensor
        objIndex[baseIndex + 2].type = OBJ_T_POWER_SENSOR;
        objIndex[baseIndex + 2].obj = &pwr_power[i];
        sprintf(objIndex[baseIndex + 2].name, "%s Power", sensorNames[i]);
        objIndex[baseIndex + 2].valid = true;

        if (!pwr_interface[i].sensor->begin()) {
            // Set fault on all three sensor objects
            pwr_voltage[i].fault = true;
            pwr_voltage[i].newMessage = true;
            sprintf(pwr_voltage[i].message, "%s power sensor init failed", sensorNames[i]);
            
            pwr_current[i].fault = true;
            pwr_current[i].newMessage = true;
            sprintf(pwr_current[i].message, "%s power sensor init failed", sensorNames[i]);
            
            pwr_power[i].fault = true;
            pwr_power[i].newMessage = true;
            sprintf(pwr_power[i].message, "%s power sensor init failed", sensorNames[i]);
            
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
        pwr_voltage[i].voltage = pwr_interface[i].sensor->volts();
        pwr_current[i].current = pwr_interface[i].sensor->amps();
        pwr_power[i].power = pwr_interface[i].sensor->watts();
    }
    return true;
}