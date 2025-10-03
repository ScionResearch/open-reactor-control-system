#include "drv_modbus_alicat_mfc.h"

ModbusAlicatMFC_t modbusAlicatMFCprobe;

bool newSetpoint = false;
float pendingSetpoint = 0.0;

void init_modbusAlicatMFCDriver(ModbusDriver_t *modbusDriver, uint8_t slaveID) {
    modbusAlicatMFCprobe.modbusDriver = modbusDriver;
    modbusAlicatMFCprobe.slaveID = slaveID;
}

void mfcResponseHandler(bool valid, uint16_t *data) {
    if (!valid) {
        modbusAlicatMFCprobe.fault = true;
        snprintf(modbusAlicatMFCprobe.message, sizeof(modbusAlicatMFCprobe.message), "Invalid data from Alicat MFC.");
        modbusAlicatMFCprobe.newMessage = true;
        return;
    }
    /************************************** 
     * Data from registers 1349-1364
     *  1349: Setpoint (float)
     *  1351: Valve Drive (float)
     *  1353: Pressure (float)
     *  1355: Secondary Pressure (float)
     *  1357: Barometric Pressure (float)
     *  1359: Temperature (float)
     *  1361: Volumetric Flow (float)
     *  1363: Mass Flow (float)
    **************************************/
    modbusAlicatMFCprobe.setpoint = modbusAlicatMFCprobe.modbusDriver->modbus.swappedUint16toFloat32(&data[0]);
    modbusAlicatMFCprobe.pressureSensor.pressure = modbusAlicatMFCprobe.modbusDriver->modbus.swappedUint16toFloat32(&data[4]);
    modbusAlicatMFCprobe.flowSensor.flow = modbusAlicatMFCprobe.modbusDriver->modbus.swappedUint16toFloat32(&data[12]);
    if (newSetpoint) {
        // Validate that the setpoint read back matches the one we tried to write
        if (fabs(modbusAlicatMFCprobe.setpoint - pendingSetpoint) > 0.01) {
            modbusAlicatMFCprobe.fault = true;
            snprintf(modbusAlicatMFCprobe.message, sizeof(modbusAlicatMFCprobe.message), "Setpoint write validation failed, expected %0.4f, got %0.4f", pendingSetpoint, modbusAlicatMFCprobe.setpoint);
            modbusAlicatMFCprobe.newMessage = true;
        } else {
            modbusAlicatMFCprobe.newMessage = true;
            snprintf(modbusAlicatMFCprobe.message, sizeof(modbusAlicatMFCprobe.message), "Setpoint write successful, setpoint is now %0.4f", modbusAlicatMFCprobe.setpoint);
        }
        newSetpoint = false;
    }
}

void mfcWriteResponseHandler(bool valid, uint16_t *data) {
    static int writeAttempts = 0;
    if (!valid) {
        if (writeAttempts < 5) {
            writeAttempts ++;
            modbusAlicatMFC_writeSP(pendingSetpoint);
        } else {
            writeAttempts = 0;
            modbusAlicatMFCprobe.fault = true;
            snprintf(modbusAlicatMFCprobe.message, sizeof(modbusAlicatMFCprobe.message), "Failed to write setpoint %0.4f to Alicat MFC", pendingSetpoint);
            modbusAlicatMFCprobe.newMessage = true;
        }
        return;
    }
    newSetpoint = true;  // Indicate that the new setpoint was written successfully so main sensor data request can validate it
    writeAttempts = 0;
}

void modbusAlicatMFC_manage() {
    uint8_t functionCode = 3;
    uint16_t address = 1349;    // Address of setpoint register
    static uint16_t data[16];
    if (!modbusAlicatMFCprobe.modbusDriver->modbus.pushRequest(modbusAlicatMFCprobe.slaveID, functionCode, address, data, 16, mfcResponseHandler)) {
        return;
    }
}

void modbusAlicatMFC_writeSP(float setpoint) {
    pendingSetpoint = setpoint;
    uint8_t functionCode = 16;
    uint16_t address = 1349;    // Address of setpoint register
    static uint16_t data[2];
    modbusAlicatMFCprobe.modbusDriver->modbus.float32ToSwappedUint16(setpoint, data);

    if (!modbusAlicatMFCprobe.modbusDriver->modbus.pushRequest(modbusAlicatMFCprobe.slaveID, functionCode, address, data, 2, mfcWriteResponseHandler)) {
        return;
    }
}