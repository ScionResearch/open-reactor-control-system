#include "drv_rtd.h"

int rtdSensorCount = 0;

struct RtdRef_t {
    int R_nom;
    int R_ref;
};

RtdRef_t rtdRefs[] = {
    {100, 400},
    {1000, 4000}
};

int rtdPins[] = {PIN_PT100_CS_1, PIN_PT100_CS_2, PIN_PT100_CS_3};
TemperatureSensor_t rtd_sensor[3];
RTDDriver_t rtd_interface[3];

bool init_rtdDriver(void) {
    // Initialise CS pins for the MAX31865 ICs
    for (int i = 0; i < NUM_MAX31865_INTERFACES; i++) {
        pinMode(rtdPins[i], OUTPUT);
        digitalWrite(rtdPins[i], HIGH);
    }

    // ----- Temp code- init other CS pins too ----- //
    pinMode(PIN_ADC_CS, OUTPUT);
    pinMode(PIN_DAC_CS, OUTPUT);

    digitalWrite(PIN_ADC_CS, HIGH);
    digitalWrite(PIN_DAC_CS, HIGH);
    // --------------- End temp code --------------- //

    // Initialise config
    for (int i = 0; i < NUM_MAX31865_INTERFACES; i++) {
        rtd_interface[i].temperatureObj = &rtd_sensor[i];
        rtd_interface[i].cs_pin = rtdPins[i];
        rtd_interface[i].sensor = NULL;
        rtd_interface[i].wires = MAX31865_3WIRE;
        rtd_interface[i].sensorType = PT100;
        rtd_interface[i].cal = &calTable[i + CAL_RTD_PTR];
    }

    // Initialise temperature sensors
    for (int i = 0; i < NUM_MAX31865_INTERFACES; i++) {
        if (!initTemperatureSensor(&rtd_interface[i])) return false;
    }
    return true;
}

bool initTemperatureSensor(RTDDriver_t *sensorObj) {
    if (rtdSensorCount >= NUM_MAX31865_INTERFACES) return false;
    rtdSensorCount ++;
    // Initialise the temperature sensor object
    sensorObj->temperatureObj->temperature = 0;
    strcpy(sensorObj->temperatureObj->unit, "°C"); // Default units for the temperature
    // Initialise the temperature sensor
    sensorObj->sensor = new Adafruit_MAX31865(sensorObj->cs_pin);
    return sensorObj->sensor->begin(sensorObj->wires);
}

bool readRtdSensors(void) {
    if (rtdSensorCount == 0) {
        Serial.println("No RTD sensors initialised.");
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
        Serial.println("Invalid RTD sensor object.");
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
    float temperature = sensorObj->sensor->temperature(rtdRefs[sensorObj->sensorType].R_nom, rtdRefs[sensorObj->sensorType].R_ref);
    temperature = (temperature * sensorObj->cal->scale) + sensorObj->cal->offset;
    sensorObj->temperatureObj->temperature = temperature;
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
