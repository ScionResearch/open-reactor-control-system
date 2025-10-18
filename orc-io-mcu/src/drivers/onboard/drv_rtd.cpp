#include "drv_rtd.h"
#include <string.h>  // For strcmp

int rtdSensorCount = 0;

struct RtdRef_t {
    int R_nom;
    int R_ref;
};

RtdRef_t rtdRefs[] = {
    {100, 400},
    {1000, 4000}
};

int rtdCSPins[] = {PIN_PT100_CS_1, PIN_PT100_CS_2, PIN_PT100_CS_3};
int rtdDRDYPins[] = {PIN_PT100_DRDY_1, PIN_PT100_DRDY_2, PIN_PT100_DRDY_3};
TemperatureSensor_t rtd_sensor[3];
RTDDriver_t rtd_interface[3];

bool init_rtdDriver(void) {
    // Initialise CS pins for the MAX31865 ICs
    for (int i = 0; i < NUM_MAX31865_INTERFACES; i++) {
        pinMode(rtdCSPins[i], OUTPUT);
        digitalWrite(rtdCSPins[i], HIGH);
    }

    // Initialise config
    for (int i = 0; i < NUM_MAX31865_INTERFACES; i++) {
        rtd_interface[i].temperatureObj = &rtd_sensor[i];
        rtd_interface[i].cs_pin = rtdCSPins[i];
        rtd_interface[i].drdy_pin = rtdDRDYPins[i];
        rtd_interface[i].sensor = NULL;
        rtd_interface[i].wires = MAX31865_3WIRE;
        rtd_interface[i].sensorType = PT100;
        rtd_interface[i].cal = &calTable[i + CAL_RTD_PTR];
        
        // Initialize temperature sensor object
        rtd_sensor[i].temperature = 0;
        strcpy(rtd_sensor[i].unit, "C");
        rtd_sensor[i].fault = false;
        rtd_sensor[i].newMessage = false;
        rtd_sensor[i].cal = &calTable[i + CAL_RTD_PTR];
        
        // Add to object index (fixed indices 10-12)
        objIndex[10 + i].type = OBJ_T_TEMPERATURE_SENSOR;
        objIndex[10 + i].obj = &rtd_sensor[i];
        sprintf(objIndex[10 + i].name, "RTD Temperature %d", i + 1);
        objIndex[10 + i].valid = true;
    }

    // Initialise temperature sensors
    for (int i = 0; i < NUM_MAX31865_INTERFACES; i++) {
        if (!initTemperatureSensor(&rtd_interface[i])) return false;
        rtd_interface[i].sensor->autoConvert(true);
        //rtd_interface[i].sensor->enable50Hz(true);
    }
    return true;
}

bool initTemperatureSensor(RTDDriver_t *sensorObj) {
    if (rtdSensorCount >= NUM_MAX31865_INTERFACES) return false;
    rtdSensorCount ++;
    // Initialise the temperature sensor hardware
    sensorObj->sensor = new MAX31865(sensorObj->cs_pin, &SPI);
    return sensorObj->sensor->begin(sensorObj->wires);
}

bool readRtdSensors(void) {
    if (rtdSensorCount == 0) {
        return false;
    }
    for (int i = 0; i < rtdSensorCount; i++) {
        if (!readRtdSensor(&rtd_interface[i])) {
            rtd_interface[i].temperatureObj->fault = true;
            return false;
        }
        rtd_interface[i].temperatureObj->fault = false;
    }
    return true;
}

bool readRtdSensor(RTDDriver_t *sensorObj) {
    if (sensorObj == NULL || sensorObj->sensor == NULL || sensorObj->temperatureObj == NULL) {
        return false;
    }
    uint8_t fault = sensorObj->sensor->readFault();
    if (fault) {
        sensorObj->temperatureObj->newMessage = true;
        char buf[100];
        snprintf(buf, 20, "RTD Fault 0x%02x ", fault);
        strcpy(sensorObj->temperatureObj->message, buf);

        if (fault & MAX31865_FAULT_HIGHTHRESH) {
            strcat(sensorObj->temperatureObj->message, "| RTD High Threshold ");
        }
        if (fault & MAX31865_FAULT_LOWTHRESH) {
            strcat(sensorObj->temperatureObj->message, "| RTD Low Threshold ");
        }
        if (fault & MAX31865_FAULT_REFINLOW) {
            strcat(sensorObj->temperatureObj->message, "| REFIN- > 0.85 x Bias "); 
        }
        if (fault & MAX31865_FAULT_REFINHIGH) {
            strcat(sensorObj->temperatureObj->message, "| REFIN- < 0.85 x Bias - FORCE- open "); 
        }
        if (fault & MAX31865_FAULT_RTDINLOW) {
            strcat(sensorObj->temperatureObj->message, "| RTDIN- < 0.85 x Bias - FORCE- open "); 
        }
        if (fault & MAX31865_FAULT_OVUV) {
            strcat(sensorObj->temperatureObj->message, "| Under/Over voltage");
        }
        sensorObj->sensor->clearFault();
    }
    // Read temperature from MAX31865 (always returns Celsius)
    float tempCelsius = sensorObj->sensor->temperature(rtdRefs[sensorObj->sensorType].R_nom, rtdRefs[sensorObj->sensorType].R_ref);
    
    // Apply calibration (scale and offset) to the Celsius reading
    tempCelsius = (tempCelsius * sensorObj->cal->scale) + sensorObj->cal->offset;
    
    // Convert to the requested unit
    float finalTemperature;
    if (strcmp(sensorObj->temperatureObj->unit, "F") == 0) {
        // Convert Celsius to Fahrenheit: F = C × 9/5 + 32
        finalTemperature = (tempCelsius * 9.0 / 5.0) + 32.0;
    } else if (strcmp(sensorObj->temperatureObj->unit, "K") == 0) {
        // Convert Celsius to Kelvin: K = C + 273.15
        finalTemperature = tempCelsius + 273.15;
    } else {
        // Default to Celsius (or if unit is explicitly "C")
        finalTemperature = tempCelsius;
    }
    
    sensorObj->temperatureObj->temperature = finalTemperature;
    return true;
}

bool setRtdSensorType(RTDDriver_t *sensorObj, RtdSensorType sensorType) {
    if (sensorObj->sensor == NULL) return false;
    sensorObj->sensorType = sensorType;
    return true;
}

bool setRtdWires(RTDDriver_t *sensorObj, max31865_numwires_t wires) {
    if (sensorObj->sensor == NULL) return false;
    sensorObj->wires = wires;
    sensorObj->sensor->setWires(wires);
    return true;
}

void RTD_manage(void) {
    readRtdSensors();
}
