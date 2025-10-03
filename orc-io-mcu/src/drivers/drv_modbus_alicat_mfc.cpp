#include "drv_modbus_alicat_mfc.h"

ModbusAlicatMFC_t modbusAlicatMFCprobe;

void init_modbusAlicatMFCDriver(ModbusDriver_t *modbusDriver, uint8_t slaveID) {
    modbusAlicatMFCprobe.modbusDriver = modbusDriver;
    modbusAlicatMFCprobe.slaveID = slaveID;
}

void mfcResponseHandler(bool valid, uint16_t *data) {
    if (!valid) {
        //Serial.println("Invalid flow sensor data.");
        return;
    }
    /* Data from registers 1349-1364
       1349: Setpoint (float)
       1351: Valve Drive (float)
       1353: Pressure (float)
       1355: Secondary Pressure (float)
       1357: Barometric Pressure (float)
       1359: Temperature (float)
       1361: Volumetric Flow (float)
       1363: Mass Flow (float)
    */
    modbusAlicatMFCprobe.setpoint = modbusAlicatMFCprobe.modbusDriver->modbus.swappedUint16toFloat32(&data[0]);
    modbusAlicatMFCprobe.pressureSensor.pressure = modbusAlicatMFCprobe.modbusDriver->modbus.swappedUint16toFloat32(&data[4]);
    modbusAlicatMFCprobe.flowSensor.flow = modbusAlicatMFCprobe.modbusDriver->modbus.swappedUint16toFloat32(&data[12]);
}

void mfcWriteResponseHandler(bool valid, uint16_t *data) {
    if (!valid) {
        //Serial.println("xxx Invalid MFC write response data. xxx");
        return;
    }
    //Serial.println("+++ MFC write response received. +++");
}

void modbusAlicatMFC_manage() {
    //Serial.println("Queuing Alicat MFC modbus requests");
    uint8_t functionCode = 3;
    uint16_t address = 1349;    // Address of setpoint register
    static uint16_t data[16];
    if (!modbusAlicatMFCprobe.modbusDriver->modbus.pushRequest(modbusAlicatMFCprobe.slaveID, functionCode, address, data, 16, mfcResponseHandler)) {
        //Serial.println("ERROR - queue full");
        return;
    }

    //Serial.printf("Current queue size: %d\n", modbusAlicatMFCprobe.modbusDriver->modbus.getQueueCount());
}

void modbusAlicatMFC_writeSP(float setpoint) {
    //Serial.printf("*** Writing Alicat MFC new setpoint: %0.4f ***\n", setpoint);
    uint8_t functionCode = 16;
    uint16_t address = 1349;    // Address of setpoint register
    static uint16_t data[2];
    modbusAlicatMFCprobe.modbusDriver->modbus.float32ToSwappedUint16(setpoint, data);

    if (!modbusAlicatMFCprobe.modbusDriver->modbus.pushRequest(modbusAlicatMFCprobe.slaveID, functionCode, address, data, 2, mfcWriteResponseHandler)) {
        //Serial.println("ERROR - queue full");
        return;
    }

    //Serial.printf("Current queue size: %d\n", modbusAlicatMFCprobe.modbusDriver->modbus.getQueueCount());
}