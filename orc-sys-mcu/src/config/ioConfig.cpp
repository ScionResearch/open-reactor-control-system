#include "ioConfig.h"
#include "../sys_init.h"  // For ipc global instance

// Global configuration instance
IOConfig ioConfig;

/**
 * @brief Set default configuration values for all IO objects
 */
void setDefaultIOConfig() {
    log(LOG_INFO, false, "Setting default IO configuration\n");
    
    // Set magic number and version
    ioConfig.magicNumber = IO_CONFIG_MAGIC_NUMBER;
    ioConfig.version = IO_CONFIG_VERSION;
    
    // ========================================================================
    // ADC Inputs (Indices 0-7)
    // ========================================================================
    for (int i = 0; i < MAX_ADC_INPUTS; i++) {
        snprintf(ioConfig.adcInputs[i].name, sizeof(ioConfig.adcInputs[i].name), 
                 "Analog Input %d", i + 1);
        strcpy(ioConfig.adcInputs[i].unit, "mV");
        ioConfig.adcInputs[i].cal.scale = 1.0;
        ioConfig.adcInputs[i].cal.offset = 0.0;
        ioConfig.adcInputs[i].enabled = true;
        ioConfig.adcInputs[i].showOnDashboard = false;
    }
    
    // ========================================================================
    // DAC Outputs (Indices 8-9)
    // ========================================================================
    for (int i = 0; i < MAX_DAC_OUTPUTS; i++) {
        snprintf(ioConfig.dacOutputs[i].name, sizeof(ioConfig.dacOutputs[i].name), 
                 "Analog Output %d", i + 1);
        strcpy(ioConfig.dacOutputs[i].unit, "mV");
        ioConfig.dacOutputs[i].cal.scale = 1.0;
        ioConfig.dacOutputs[i].cal.offset = 0.0;
        ioConfig.dacOutputs[i].enabled = true;
        ioConfig.dacOutputs[i].showOnDashboard = false;
    }
    
    // ========================================================================
    // RTD Temperature Sensors (Indices 10-12)
    // ========================================================================
    for (int i = 0; i < MAX_RTD_SENSORS; i++) {
        snprintf(ioConfig.rtdSensors[i].name, sizeof(ioConfig.rtdSensors[i].name), 
                 "RTD Temperature %d", i + 1);
        strcpy(ioConfig.rtdSensors[i].unit, "C");
        ioConfig.rtdSensors[i].cal.scale = 1.0;
        ioConfig.rtdSensors[i].cal.offset = 0.0;
        ioConfig.rtdSensors[i].wireConfig = 3;      // 3-wire by default
        ioConfig.rtdSensors[i].nominalOhms = 100;   // PT100 by default
        ioConfig.rtdSensors[i].enabled = true;
        ioConfig.rtdSensors[i].showOnDashboard = false;
    }
    
    // ========================================================================
    // Digital Inputs (Indices 13-20)
    // ========================================================================
    for (int i = 0; i < MAX_GPIO; i++) {
        snprintf(ioConfig.gpio[i].name, sizeof(ioConfig.gpio[i].name), 
                 "Input %d", i + 1);  // Label as Input 1-8 to match board
        ioConfig.gpio[i].pullMode = GPIO_PULL_UP;  // Default to pull-up
        ioConfig.gpio[i].enabled = true;
        ioConfig.gpio[i].showOnDashboard = false;
    }
    
    // ========================================================================
    // Digital Outputs (Indices 21-25: Open Drain 1-4, High Current)
    // ========================================================================
    const char* outputNames[] = {"Output 1", "Output 2", "Output 3", "Output 4", "Heater Output"};
    for (int i = 0; i < MAX_DIGITAL_OUTPUTS; i++) {
        strlcpy(ioConfig.digitalOutputs[i].name, outputNames[i], 
                sizeof(ioConfig.digitalOutputs[i].name));
        ioConfig.digitalOutputs[i].mode = OUTPUT_MODE_ON_OFF;
        ioConfig.digitalOutputs[i].enabled = true;
        ioConfig.digitalOutputs[i].showOnDashboard = false;
    }
    
    // ========================================================================
    // Stepper Motor (Index 26)
    // ========================================================================
    strlcpy(ioConfig.stepperMotor.name, "Stepper Motor", 
            sizeof(ioConfig.stepperMotor.name));
    ioConfig.stepperMotor.stepsPerRev = 200;
    ioConfig.stepperMotor.maxRPM = 500;
    ioConfig.stepperMotor.holdCurrent_mA = 50;   // Safe default: 50mA hold current
    ioConfig.stepperMotor.runCurrent_mA = 100;   // Safe default: 100mA run current
    ioConfig.stepperMotor.acceleration = 100;
    ioConfig.stepperMotor.invertDirection = false;
    ioConfig.stepperMotor.enabled = true;
    ioConfig.stepperMotor.showOnDashboard = false;
    
    // ========================================================================
    // DC Motors (Indices 27-30)
    // ========================================================================
    for (int i = 0; i < MAX_DC_MOTORS; i++) {
        snprintf(ioConfig.dcMotors[i].name, sizeof(ioConfig.dcMotors[i].name), 
                 "DC Motor %d", i + 1);
        ioConfig.dcMotors[i].invertDirection = false;
        ioConfig.dcMotors[i].enabled = true;
        ioConfig.dcMotors[i].showOnDashboard = false;
    }
    
    // ========================================================================
    // Energy Sensors (Indices 31-32)
    // ========================================================================
    const char* energySensorNames[] = {"Main Power Monitor", "Heater Power Monitor"};
    for (int i = 0; i < MAX_ENERGY_SENSORS; i++) {
        strlcpy(ioConfig.energySensors[i].name, energySensorNames[i], 
                sizeof(ioConfig.energySensors[i].name));
        ioConfig.energySensors[i].showOnDashboard = false;
    }
    
    // ========================================================================
    // Temperature Controllers (Indices 40-49)
    // ========================================================================
    for (int i = 0; i < MAX_TEMP_CONTROLLERS; i++) {
        ioConfig.tempControllers[i].isActive = false;
        snprintf(ioConfig.tempControllers[i].name, sizeof(ioConfig.tempControllers[i].name), 
                 "Temperature Controller %d", i + 1);
        ioConfig.tempControllers[i].enabled = false;
        ioConfig.tempControllers[i].showOnDashboard = false;
        strlcpy(ioConfig.tempControllers[i].unit, "C", sizeof(ioConfig.tempControllers[i].unit));
        
        ioConfig.tempControllers[i].pvSourceIndex = 0;    // Default: no sensor assigned
        ioConfig.tempControllers[i].outputIndex = 0;      // Default: no output assigned
        
        ioConfig.tempControllers[i].controlMethod = CONTROL_METHOD_PID;
        ioConfig.tempControllers[i].setpoint = 25.0f;     // Default 25°C
        
        // On/Off defaults
        ioConfig.tempControllers[i].hysteresis = 0.5f;    // 0.5°C deadband
        
        // PID defaults (conservative starting values)
        ioConfig.tempControllers[i].kP = 2.0f;
        ioConfig.tempControllers[i].kI = 0.5f;
        ioConfig.tempControllers[i].kD = 0.1f;
        ioConfig.tempControllers[i].integralWindup = 100.0f;
        ioConfig.tempControllers[i].outputMin = 0.0f;
        ioConfig.tempControllers[i].outputMax = 100.0f;
    }
    
    // ========================================================================
    // COM Ports (0-1: RS-232, 2-3: RS-485)
    // ========================================================================
    const char* portNames[] = {"RS-232 Port 1", "RS-232 Port 2", "RS-485 Port 1", "RS-485 Port 2"};
    for (int i = 0; i < MAX_COM_PORTS; i++) {
        strlcpy(ioConfig.comPorts[i].name, portNames[i], sizeof(ioConfig.comPorts[i].name));
        ioConfig.comPorts[i].baudRate = 9600;
        ioConfig.comPorts[i].dataBits = 8;
        ioConfig.comPorts[i].stopBits = 1.0;
        ioConfig.comPorts[i].parity = 2;  // Even parity (standard for Modbus)
        ioConfig.comPorts[i].enabled = true;
        ioConfig.comPorts[i].showOnDashboard = false;
    }
    
    // ========================================================================
    // Devices (Dynamic indices 60-79)
    // ========================================================================
    for (int i = 0; i < MAX_DEVICES; i++) {
        ioConfig.devices[i].isActive = false;
        ioConfig.devices[i].dynamicIndex = 0xFF;  // Unassigned
        ioConfig.devices[i].name[0] = '\0';
    }
    
    // ========================================================================
    // Device Sensors (Indices 60-79)
    // ========================================================================
    for (int i = 0; i < MAX_DEVICE_SENSORS; i++) {
        ioConfig.deviceSensors[i].name[0] = '\0';  // Empty by default (use IO MCU name)
        ioConfig.deviceSensors[i].showOnDashboard = false;
        ioConfig.deviceSensors[i].nameOverridden = false;
    }
}

/**
 * @brief Load IO configuration from LittleFS
 * @return true if successful, false if file not found or invalid
 */
bool loadIOConfig() {
    log(LOG_INFO, true, "Loading IO configuration from %s\n", IO_CONFIG_FILENAME);
    
    // CRITICAL: Initialize defaults FIRST to ensure all fields have safe values
    // This prevents crashes from uninitialized memory if config file is missing sections
    setDefaultIOConfig();
    
    // Check if LittleFS is mounted
    if (!LittleFS.begin()) {
        log(LOG_WARNING, true, "Failed to mount LittleFS\n");
        return false;
    }
    
    // Check if config file exists
    if (!LittleFS.exists(IO_CONFIG_FILENAME)) {
        log(LOG_WARNING, true, "IO config file not found, using defaults\n");
        LittleFS.end();
        return false;
    }
    
    // Open config file
    File configFile = LittleFS.open(IO_CONFIG_FILENAME, "r");
    if (!configFile) {
        log(LOG_WARNING, true, "Failed to open IO config file\n");
        LittleFS.end();
        return false;
    }
    
    // Allocate JSON document on heap (sized for our config)
    DynamicJsonDocument doc(16384);
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();
    LittleFS.end();
    
    if (error) {
        log(LOG_WARNING, true, "Failed to parse IO config: %s\n", error.c_str());
        return false;
    } else {
        log(LOG_INFO, false, "Deserialized IO config file: %d bytes\n", doc.memoryUsage());
    }
    
    // Check magic number and version
    uint8_t magic = doc["magic"] | 0;
    uint8_t version = doc["version"] | 0;
    
    if (magic != IO_CONFIG_MAGIC_NUMBER) {
        log(LOG_WARNING, true, "Invalid magic number in IO config: 0x%02X\n", magic);
        return false;
    }
    
    if (version != IO_CONFIG_VERSION) {
        log(LOG_WARNING, true, "IO config version mismatch: %d (expected %d)\n", 
            version, IO_CONFIG_VERSION);
        return false;
    }
    
    log(LOG_INFO, true, "IO config valid (version %d)\n", version);
    
    // ========================================================================
    // Parse ADC Inputs
    // ========================================================================
    JsonArray adcArray = doc["adc_inputs"];
    if (adcArray) {
        for (int i = 0; i < MAX_ADC_INPUTS && i < adcArray.size(); i++) {
            JsonObject adc = adcArray[i];
            strlcpy(ioConfig.adcInputs[i].name, adc["name"] | "", 
                    sizeof(ioConfig.adcInputs[i].name));
            strlcpy(ioConfig.adcInputs[i].unit, adc["unit"] | "mV", 
                    sizeof(ioConfig.adcInputs[i].unit));
            ioConfig.adcInputs[i].cal.scale = adc["cal_scale"] | 1.0;
            ioConfig.adcInputs[i].cal.offset = adc["cal_offset"] | 0.0;
            ioConfig.adcInputs[i].enabled = adc["enabled"] | true;
            ioConfig.adcInputs[i].showOnDashboard = adc["showOnDashboard"] | false;
        }
    }
    
    // ========================================================================
    // Parse DAC Outputs
    // ========================================================================
    JsonArray dacArray = doc["dac_outputs"];
    if (dacArray) {
        for (int i = 0; i < MAX_DAC_OUTPUTS && i < dacArray.size(); i++) {
            JsonObject dac = dacArray[i];
            strlcpy(ioConfig.dacOutputs[i].name, dac["name"] | "", 
                    sizeof(ioConfig.dacOutputs[i].name));
            strlcpy(ioConfig.dacOutputs[i].unit, dac["unit"] | "mV", 
                    sizeof(ioConfig.dacOutputs[i].unit));
            ioConfig.dacOutputs[i].cal.scale = dac["cal_scale"] | 1.0;
            ioConfig.dacOutputs[i].cal.offset = dac["cal_offset"] | 0.0;
            ioConfig.dacOutputs[i].enabled = dac["enabled"] | true;
            ioConfig.dacOutputs[i].showOnDashboard = dac["showOnDashboard"] | false;
        }
    }
    
    // ========================================================================
    // Parse RTD Sensors
    // ========================================================================
    JsonArray rtdArray = doc["rtd_sensors"];
    if (rtdArray) {
        for (int i = 0; i < MAX_RTD_SENSORS && i < rtdArray.size(); i++) {
            JsonObject rtd = rtdArray[i];
            strlcpy(ioConfig.rtdSensors[i].name, rtd["name"] | "", 
                    sizeof(ioConfig.rtdSensors[i].name));
            strlcpy(ioConfig.rtdSensors[i].unit, rtd["unit"] | "C", 
                    sizeof(ioConfig.rtdSensors[i].unit));
            ioConfig.rtdSensors[i].cal.scale = rtd["cal"]["scale"] | 1.0;
            ioConfig.rtdSensors[i].cal.offset = rtd["cal"]["offset"] | 0.0;
            ioConfig.rtdSensors[i].wireConfig = rtd["wire_config"] | 3;
            ioConfig.rtdSensors[i].nominalOhms = rtd["nominal_ohms"] | 100;
            ioConfig.rtdSensors[i].enabled = rtd["enabled"] | true;
            ioConfig.rtdSensors[i].showOnDashboard = rtd["showOnDashboard"] | false;
        }
    }
    
    // ========================================================================
    // Parse GPIO
    // ========================================================================
    JsonArray gpioArray = doc["gpio"];
    if (gpioArray) {
        for (int i = 0; i < MAX_GPIO && i < gpioArray.size(); i++) {
            JsonObject gpio = gpioArray[i];
            strlcpy(ioConfig.gpio[i].name, gpio["name"] | "", 
                    sizeof(ioConfig.gpio[i].name));
            ioConfig.gpio[i].pullMode = (GPIOPullMode)(gpio["pullMode"] | GPIO_PULL_UP);
            ioConfig.gpio[i].enabled = gpio["enabled"] | true;
            ioConfig.gpio[i].showOnDashboard = gpio["showOnDashboard"] | false;
        }
    }
    
    // ========================================================================
    // Parse Digital Outputs (open drain + high current)
    // ========================================================================
    JsonArray outputArray = doc["digital_outputs"];
    if (outputArray) {
        for (int i = 0; i < MAX_DIGITAL_OUTPUTS && i < outputArray.size(); i++) {
            JsonObject output = outputArray[i];
            strlcpy(ioConfig.digitalOutputs[i].name, output["name"] | "", 
                    sizeof(ioConfig.digitalOutputs[i].name));
            ioConfig.digitalOutputs[i].mode = (OutputMode)(output["mode"] | OUTPUT_MODE_ON_OFF);
            ioConfig.digitalOutputs[i].enabled = output["enabled"] | true;
            ioConfig.digitalOutputs[i].showOnDashboard = output["showOnDashboard"] | false;
        }
    }
    
    // ========================================================================
    // Parse Stepper Motor
    // ========================================================================
    JsonObject stepper = doc["stepper_motor"];
    if (stepper) {
        strlcpy(ioConfig.stepperMotor.name, stepper["name"] | "Stepper Motor", 
                sizeof(ioConfig.stepperMotor.name));
        ioConfig.stepperMotor.stepsPerRev = stepper["stepsPerRev"] | 200;
        ioConfig.stepperMotor.maxRPM = stepper["maxRPM"] | 500;
        ioConfig.stepperMotor.holdCurrent_mA = stepper["holdCurrent_mA"] | 50;   // Safe default: 50mA
        ioConfig.stepperMotor.runCurrent_mA = stepper["runCurrent_mA"] | 100;    // Safe default: 100mA
        ioConfig.stepperMotor.acceleration = stepper["acceleration"] | 100;
        ioConfig.stepperMotor.invertDirection = stepper["invertDirection"] | false;
        ioConfig.stepperMotor.enabled = stepper["enabled"] | true;
        ioConfig.stepperMotor.showOnDashboard = stepper["showOnDashboard"] | false;
    }
    
    // ========================================================================
    // Parse DC Motors
    // ========================================================================
    JsonArray dcMotorArray = doc["dc_motors"];
    if (dcMotorArray) {
        for (int i = 0; i < MAX_DC_MOTORS && i < dcMotorArray.size(); i++) {
            JsonObject motor = dcMotorArray[i];
            strlcpy(ioConfig.dcMotors[i].name, motor["name"] | "", 
                    sizeof(ioConfig.dcMotors[i].name));
            ioConfig.dcMotors[i].invertDirection = motor["invertDirection"] | false;
            ioConfig.dcMotors[i].enabled = motor["enabled"] | true;
            ioConfig.dcMotors[i].showOnDashboard = motor["showOnDashboard"] | false;
        }
    }
    
    // ========================================================================
    // Parse Energy Sensors
    // ========================================================================
    JsonArray energyArray = doc["energy_sensors"];
    if (energyArray) {
        for (int i = 0; i < MAX_ENERGY_SENSORS && i < energyArray.size(); i++) {
            JsonObject sensor = energyArray[i];
            strlcpy(ioConfig.energySensors[i].name, sensor["name"] | "", 
                    sizeof(ioConfig.energySensors[i].name));
            ioConfig.energySensors[i].showOnDashboard = sensor["showOnDashboard"] | true;
        }
    }
    
    // ========================================================================
    // Parse Temperature Controllers
    // ========================================================================
    JsonArray tempCtrlArray = doc["temp_controllers"];
    if (tempCtrlArray) {
        for (int i = 0; i < MAX_TEMP_CONTROLLERS && i < tempCtrlArray.size(); i++) {
            JsonObject ctrl = tempCtrlArray[i];
            ioConfig.tempControllers[i].isActive = ctrl["isActive"] | false;
            strlcpy(ioConfig.tempControllers[i].name, ctrl["name"] | "", 
                    sizeof(ioConfig.tempControllers[i].name));
            ioConfig.tempControllers[i].enabled = ctrl["enabled"] | false;
            ioConfig.tempControllers[i].showOnDashboard = ctrl["showOnDashboard"] | false;
            strlcpy(ioConfig.tempControllers[i].unit, ctrl["unit"] | "C", 
                    sizeof(ioConfig.tempControllers[i].unit));
            
            ioConfig.tempControllers[i].pvSourceIndex = ctrl["pvSourceIndex"] | 0;
            ioConfig.tempControllers[i].outputIndex = ctrl["outputIndex"] | 0;
            
            ioConfig.tempControllers[i].controlMethod = (ControlMethod)(ctrl["controlMethod"] | CONTROL_METHOD_PID);
            ioConfig.tempControllers[i].setpoint = ctrl["setpoint"] | 25.0f;
            
            ioConfig.tempControllers[i].hysteresis = ctrl["hysteresis"] | 0.5f;
            
            ioConfig.tempControllers[i].kP = ctrl["kP"] | 2.0f;
            ioConfig.tempControllers[i].kI = ctrl["kI"] | 0.5f;
            ioConfig.tempControllers[i].kD = ctrl["kD"] | 0.1f;
            ioConfig.tempControllers[i].integralWindup = ctrl["integralWindup"] | 100.0f;
            ioConfig.tempControllers[i].outputMin = ctrl["outputMin"] | 0.0f;
            ioConfig.tempControllers[i].outputMax = ctrl["outputMax"] | 100.0f;
        }
    }
    
    // ========================================================================
    // Parse COM Ports
    // ========================================================================
    JsonArray comPortArray = doc["com_ports"];
    if (comPortArray) {
        for (int i = 0; i < MAX_COM_PORTS && i < comPortArray.size(); i++) {
            JsonObject port = comPortArray[i];
            strlcpy(ioConfig.comPorts[i].name, port["name"] | "", 
                    sizeof(ioConfig.comPorts[i].name));
            ioConfig.comPorts[i].baudRate = port["baudRate"] | 9600;
            ioConfig.comPorts[i].dataBits = port["dataBits"] | 8;
            ioConfig.comPorts[i].stopBits = port["stopBits"] | 1.0;
            ioConfig.comPorts[i].parity = port["parity"] | 2;
            ioConfig.comPorts[i].enabled = port["enabled"] | true;
            ioConfig.comPorts[i].showOnDashboard = port["showOnDashboard"] | false;
        }
    }
    
    // ========================================================================
    // Parse Devices
    // ========================================================================
    JsonArray devicesArray = doc["devices"];
    if (devicesArray) {
        for (int i = 0; i < MAX_DEVICES && i < devicesArray.size(); i++) {
            JsonObject dev = devicesArray[i];
            ioConfig.devices[i].isActive = dev["isActive"] | false;
            ioConfig.devices[i].dynamicIndex = dev["dynamicIndex"] | 0xFF;
            ioConfig.devices[i].interfaceType = (DeviceInterfaceType)(dev["interfaceType"] | 0);
            ioConfig.devices[i].driverType = (DeviceDriverType)(dev["driverType"] | 0);
            strlcpy(ioConfig.devices[i].name, dev["name"] | "", 
                    sizeof(ioConfig.devices[i].name));
            
            // Parse interface-specific parameters based on interface type
            if (ioConfig.devices[i].interfaceType == DEVICE_INTERFACE_MODBUS_RTU) {
                ioConfig.devices[i].modbus.portIndex = dev["portIndex"] | 0;
                ioConfig.devices[i].modbus.slaveID = dev["slaveID"] | 1;
            } else if (ioConfig.devices[i].interfaceType == DEVICE_INTERFACE_ANALOGUE_IO) {
                ioConfig.devices[i].analogueIO.dacOutputIndex = dev["dacOutputIndex"] | 0;
                strlcpy(ioConfig.devices[i].analogueIO.unit, dev["unit"] | "bar", 
                        sizeof(ioConfig.devices[i].analogueIO.unit));
                ioConfig.devices[i].analogueIO.scale = dev["scale"] | 100.0f;
                ioConfig.devices[i].analogueIO.offset = dev["offset"] | 0.0f;
            } else if (ioConfig.devices[i].interfaceType == DEVICE_INTERFACE_MOTOR_DRIVEN) {
                ioConfig.devices[i].motorDriven.usesStepper = dev["usesStepper"] | false;
                ioConfig.devices[i].motorDriven.motorIndex = dev["motorIndex"] | 27;
            }
        }
    }
    
    // ========================================================================
    // Parse Device Sensors (Indices 60-79)
    // ========================================================================
    JsonArray deviceSensorsArray = doc["device_sensors"];
    if (deviceSensorsArray) {
        for (int i = 0; i < MAX_DEVICE_SENSORS && i < deviceSensorsArray.size(); i++) {
            JsonObject sensor = deviceSensorsArray[i];
            strlcpy(ioConfig.deviceSensors[i].name, sensor["name"] | "", 
                    sizeof(ioConfig.deviceSensors[i].name));
            ioConfig.deviceSensors[i].showOnDashboard = sensor["showOnDashboard"] | false;
            ioConfig.deviceSensors[i].nameOverridden = sensor["nameOverridden"] | false;
        }
    }
    
    log(LOG_INFO, true, "IO configuration loaded successfully\n");
    return true;
}

/**
 * @brief Save IO configuration to LittleFS
 */
void saveIOConfig() {
    log(LOG_INFO, true, "Saving IO configuration to %s\n", IO_CONFIG_FILENAME);
    
    // Check if LittleFS is mounted
    if (!LittleFS.begin()) {
        log(LOG_WARNING, true, "Failed to mount LittleFS\n");
        return;
    }
    
    // Create JSON document on heap to avoid stack overflow
    DynamicJsonDocument doc(16384);
    
    // Store magic number and version
    doc["magic"] = IO_CONFIG_MAGIC_NUMBER;
    doc["version"] = IO_CONFIG_VERSION;
    
    // ========================================================================
    // Serialize ADC Inputs
    // ========================================================================
    JsonArray adcArray = doc.createNestedArray("adc_inputs");
    for (int i = 0; i < MAX_ADC_INPUTS; i++) {
        JsonObject adc = adcArray.createNestedObject();
        adc["name"] = ioConfig.adcInputs[i].name;
        adc["unit"] = ioConfig.adcInputs[i].unit;
        adc["cal_scale"] = ioConfig.adcInputs[i].cal.scale;
        adc["cal_offset"] = ioConfig.adcInputs[i].cal.offset;
        adc["enabled"] = ioConfig.adcInputs[i].enabled;
        adc["showOnDashboard"] = ioConfig.adcInputs[i].showOnDashboard;
    }
    
    // ========================================================================
    // Serialize DAC Outputs
    // ========================================================================
    JsonArray dacArray = doc.createNestedArray("dac_outputs");
    for (int i = 0; i < MAX_DAC_OUTPUTS; i++) {
        JsonObject dac = dacArray.createNestedObject();
        dac["name"] = ioConfig.dacOutputs[i].name;
        dac["unit"] = ioConfig.dacOutputs[i].unit;
        dac["cal_scale"] = ioConfig.dacOutputs[i].cal.scale;
        dac["cal_offset"] = ioConfig.dacOutputs[i].cal.offset;
        dac["enabled"] = ioConfig.dacOutputs[i].enabled;
        dac["showOnDashboard"] = ioConfig.dacOutputs[i].showOnDashboard;
    }
    
    // ========================================================================
    // Serialize RTD Sensors
    // ========================================================================
    JsonArray rtdArray = doc.createNestedArray("rtd_sensors");
    for (int i = 0; i < MAX_RTD_SENSORS; i++) {
        JsonObject rtd = rtdArray.createNestedObject();
        rtd["name"] = ioConfig.rtdSensors[i].name;
        rtd["unit"] = ioConfig.rtdSensors[i].unit;
        JsonObject cal = rtd.createNestedObject("cal");
        cal["scale"] = ioConfig.rtdSensors[i].cal.scale;
        cal["offset"] = ioConfig.rtdSensors[i].cal.offset;
        rtd["wire_config"] = ioConfig.rtdSensors[i].wireConfig;
        rtd["nominal_ohms"] = ioConfig.rtdSensors[i].nominalOhms;
        rtd["enabled"] = ioConfig.rtdSensors[i].enabled;
        rtd["showOnDashboard"] = ioConfig.rtdSensors[i].showOnDashboard;
    }
    
    // ========================================================================
    // Serialize GPIO
    // ========================================================================
    JsonArray gpioArray = doc.createNestedArray("gpio");
    for (int i = 0; i < MAX_GPIO; i++) {
        JsonObject gpio = gpioArray.createNestedObject();
        gpio["name"] = ioConfig.gpio[i].name;
        gpio["pullMode"] = (uint8_t)ioConfig.gpio[i].pullMode;
        gpio["enabled"] = ioConfig.gpio[i].enabled;
        gpio["showOnDashboard"] = ioConfig.gpio[i].showOnDashboard;
    }
    
    // ========================================================================
    // Serialize Digital Outputs (open drain + high current)
    // ========================================================================
    JsonArray outputArray = doc.createNestedArray("digital_outputs");
    for (int i = 0; i < MAX_DIGITAL_OUTPUTS; i++) {
        JsonObject output = outputArray.createNestedObject();
        output["name"] = ioConfig.digitalOutputs[i].name;
        output["mode"] = (uint8_t)ioConfig.digitalOutputs[i].mode;
        output["enabled"] = ioConfig.digitalOutputs[i].enabled;
        output["showOnDashboard"] = ioConfig.digitalOutputs[i].showOnDashboard;
    }
    
    // ========================================================================
    // Serialize Stepper Motor
    // ========================================================================
    JsonObject stepper = doc.createNestedObject("stepper_motor");
    stepper["name"] = ioConfig.stepperMotor.name;
    stepper["stepsPerRev"] = ioConfig.stepperMotor.stepsPerRev;
    stepper["maxRPM"] = ioConfig.stepperMotor.maxRPM;
    stepper["holdCurrent_mA"] = ioConfig.stepperMotor.holdCurrent_mA;
    stepper["runCurrent_mA"] = ioConfig.stepperMotor.runCurrent_mA;
    stepper["acceleration"] = ioConfig.stepperMotor.acceleration;
    stepper["invertDirection"] = ioConfig.stepperMotor.invertDirection;
    stepper["enabled"] = ioConfig.stepperMotor.enabled;
    stepper["showOnDashboard"] = ioConfig.stepperMotor.showOnDashboard;
    
    // ========================================================================
    // Serialize DC Motors
    // ========================================================================
    JsonArray dcMotorArray = doc.createNestedArray("dc_motors");
    for (int i = 0; i < MAX_DC_MOTORS; i++) {
        JsonObject motor = dcMotorArray.createNestedObject();
        motor["name"] = ioConfig.dcMotors[i].name;
        motor["invertDirection"] = ioConfig.dcMotors[i].invertDirection;
        motor["enabled"] = ioConfig.dcMotors[i].enabled;
        motor["showOnDashboard"] = ioConfig.dcMotors[i].showOnDashboard;
    }
    
    // ========================================================================
    // Serialize Energy Sensors
    // ========================================================================
    JsonArray energyArray = doc.createNestedArray("energy_sensors");
    for (int i = 0; i < MAX_ENERGY_SENSORS; i++) {
        JsonObject sensor = energyArray.createNestedObject();
        sensor["name"] = ioConfig.energySensors[i].name;
        sensor["showOnDashboard"] = ioConfig.energySensors[i].showOnDashboard;
    }
    
    // ========================================================================
    // Serialize Temperature Controllers
    // ========================================================================
    JsonArray tempCtrlArray = doc.createNestedArray("temp_controllers");
    for (int i = 0; i < MAX_TEMP_CONTROLLERS; i++) {
        JsonObject ctrl = tempCtrlArray.createNestedObject();
        ctrl["isActive"] = ioConfig.tempControllers[i].isActive;
        ctrl["name"] = ioConfig.tempControllers[i].name;
        ctrl["enabled"] = ioConfig.tempControllers[i].enabled;
        ctrl["showOnDashboard"] = ioConfig.tempControllers[i].showOnDashboard;
        ctrl["unit"] = ioConfig.tempControllers[i].unit;
        
        ctrl["pvSourceIndex"] = ioConfig.tempControllers[i].pvSourceIndex;
        ctrl["outputIndex"] = ioConfig.tempControllers[i].outputIndex;
        
        ctrl["controlMethod"] = (uint8_t)ioConfig.tempControllers[i].controlMethod;
        ctrl["setpoint"] = ioConfig.tempControllers[i].setpoint;
        
        ctrl["hysteresis"] = ioConfig.tempControllers[i].hysteresis;
        
        ctrl["kP"] = ioConfig.tempControllers[i].kP;
        ctrl["kI"] = ioConfig.tempControllers[i].kI;
        ctrl["kD"] = ioConfig.tempControllers[i].kD;
        ctrl["integralWindup"] = ioConfig.tempControllers[i].integralWindup;
        ctrl["outputMin"] = ioConfig.tempControllers[i].outputMin;
        ctrl["outputMax"] = ioConfig.tempControllers[i].outputMax;
    }
    
    // ========================================================================
    // Serialize COM Ports
    // ========================================================================
    JsonArray comPortArray = doc.createNestedArray("com_ports");
    for (int i = 0; i < MAX_COM_PORTS; i++) {
        JsonObject port = comPortArray.createNestedObject();
        port["name"] = ioConfig.comPorts[i].name;
        port["baudRate"] = ioConfig.comPorts[i].baudRate;
        port["dataBits"] = ioConfig.comPorts[i].dataBits;
        port["stopBits"] = ioConfig.comPorts[i].stopBits;
        port["parity"] = ioConfig.comPorts[i].parity;
        port["enabled"] = ioConfig.comPorts[i].enabled;
        port["showOnDashboard"] = ioConfig.comPorts[i].showOnDashboard;
    }
    
    // ========================================================================
    // Serialize Devices
    // ========================================================================
    JsonArray devicesArray = doc.createNestedArray("devices");
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (!ioConfig.devices[i].isActive) continue;  // Skip inactive devices
        
        JsonObject dev = devicesArray.createNestedObject();
        dev["isActive"] = ioConfig.devices[i].isActive;
        dev["dynamicIndex"] = ioConfig.devices[i].dynamicIndex;
        dev["interfaceType"] = (uint8_t)ioConfig.devices[i].interfaceType;
        dev["driverType"] = (uint8_t)ioConfig.devices[i].driverType;
        dev["name"] = ioConfig.devices[i].name;
        
        // Serialize interface-specific parameters
        if (ioConfig.devices[i].interfaceType == DEVICE_INTERFACE_MODBUS_RTU) {
            dev["portIndex"] = ioConfig.devices[i].modbus.portIndex;
            dev["slaveID"] = ioConfig.devices[i].modbus.slaveID;
        } else if (ioConfig.devices[i].interfaceType == DEVICE_INTERFACE_ANALOGUE_IO) {
            dev["dacOutputIndex"] = ioConfig.devices[i].analogueIO.dacOutputIndex;
            dev["unit"] = ioConfig.devices[i].analogueIO.unit;
            dev["scale"] = ioConfig.devices[i].analogueIO.scale;
            dev["offset"] = ioConfig.devices[i].analogueIO.offset;
        } else if (ioConfig.devices[i].interfaceType == DEVICE_INTERFACE_MOTOR_DRIVEN) {
            dev["usesStepper"] = ioConfig.devices[i].motorDriven.usesStepper;
            dev["motorIndex"] = ioConfig.devices[i].motorDriven.motorIndex;
        }
    }
    
    // ========================================================================
    // Serialize Device Sensors (Indices 60-79)
    // ========================================================================
    JsonArray deviceSensorsArray = doc.createNestedArray("device_sensors");
    for (int i = 0; i < MAX_DEVICE_SENSORS; i++) {
        JsonObject sensor = deviceSensorsArray.createNestedObject();
        sensor["name"] = ioConfig.deviceSensors[i].name;
        sensor["showOnDashboard"] = ioConfig.deviceSensors[i].showOnDashboard;
        sensor["nameOverridden"] = ioConfig.deviceSensors[i].nameOverridden;
    }
    
    // Open file for writing
    File configFile = LittleFS.open(IO_CONFIG_FILENAME, "w");
    if (!configFile) {
        log(LOG_WARNING, true, "Failed to open IO config file for writing\n");
        LittleFS.end();
        return;
    }
    
    // Write to file
    if (serializeJson(doc, configFile) == 0) {
        log(LOG_WARNING, true, "Failed to write IO config file\n");
    } else {
        log(LOG_INFO, true, "IO configuration saved successfully\n");
    }

    log(LOG_DEBUG, false, "IO configuration JSON doc size: %d bytes\n", doc.memoryUsage());
    
    configFile.close();
    // Don't end LittleFS here as it will prevent serving web files
}

/**
 * @brief Print current IO configuration for debugging
 */
void printIOConfig() {
    log(LOG_INFO, true, "\n=== IO Configuration ===\n");
    log(LOG_INFO, true, "Magic: 0x%02X, Version: %d\n\n", 
        ioConfig.magicNumber, ioConfig.version);
    
    // ADC Inputs
    log(LOG_INFO, true, "ADC Inputs:\n");
    for (int i = 0; i < MAX_ADC_INPUTS; i++) {
        log(LOG_INFO, true, "  [%d] %s: %s (scale=%.3f, offset=%.3f) %s\n",
            i, ioConfig.adcInputs[i].name, ioConfig.adcInputs[i].unit,
            ioConfig.adcInputs[i].cal.scale, ioConfig.adcInputs[i].cal.offset,
            ioConfig.adcInputs[i].enabled ? "ENABLED" : "DISABLED");
    }
    
    // RTD Sensors
    log(LOG_INFO, true, "\nRTD Sensors:\n");
    for (int i = 0; i < MAX_RTD_SENSORS; i++) {
        log(LOG_INFO, true, "  [%d] %s: %s, %d-wire PT%d (scale=%.4f, offset=%.2f) %s\n",
            i + 10, ioConfig.rtdSensors[i].name, ioConfig.rtdSensors[i].unit,
            ioConfig.rtdSensors[i].wireConfig, ioConfig.rtdSensors[i].nominalOhms,
            ioConfig.rtdSensors[i].cal.scale, ioConfig.rtdSensors[i].cal.offset,
            ioConfig.rtdSensors[i].enabled ? "ENABLED" : "DISABLED");
    }
    
    // GPIO
    log(LOG_INFO, true, "\nDigital Inputs (GPIO):\n");
    for (int i = 0; i < MAX_GPIO; i++) {
        const char* pullStr = (ioConfig.gpio[i].pullMode == GPIO_PULL_UP) ? "PULL-UP" :
                              (ioConfig.gpio[i].pullMode == GPIO_PULL_DOWN) ? "PULL-DOWN" : "HIGH-Z";
        log(LOG_INFO, true, "  [%d] %s (%s) %s\n",
            i + 13, ioConfig.gpio[i].name, pullStr,
            ioConfig.gpio[i].enabled ? "ENABLED" : "DISABLED");
    }
    
    log(LOG_INFO, true, "========================\n\n");
}

/**
 * @brief Push IO configuration to IO MCU via IPC
 * Sends object-specific configuration for all enabled objects
 */
void pushIOConfigToIOmcu() {
    log(LOG_INFO, false, "Pushing IO configuration to IO MCU...\n");
    
    uint16_t sentCount = 0;
    
    // ========================================================================
    // Push ADC Input configurations (indices 0-7)
    // ========================================================================
    for (int i = 0; i < MAX_ADC_INPUTS; i++) {
        if (!ioConfig.adcInputs[i].enabled) continue;
        
        IPC_ConfigAnalogInput_t cfg;
        cfg.index = i;
        strncpy(cfg.unit, ioConfig.adcInputs[i].unit, sizeof(cfg.unit) - 1);
        cfg.unit[sizeof(cfg.unit) - 1] = '\0';
        cfg.calScale = ioConfig.adcInputs[i].cal.scale;
        cfg.calOffset = ioConfig.adcInputs[i].cal.offset;
        
        // Retry up to 10 times if queue is full
        bool sent = false;
        for (int retry = 0; retry < 10; retry++) {
            if (ipc.sendPacket(IPC_MSG_CONFIG_ANALOG_INPUT, (uint8_t*)&cfg, sizeof(cfg))) {
                sent = true;
                sentCount++;
                log(LOG_DEBUG, false, "  → ADC[%d]: %s, scale=%.3f, offset=%.3f\n",
                    i, cfg.unit, cfg.calScale, cfg.calOffset);
                break;
            }
            ipc.update();  // Process any pending RX/TX
            delay(10);  // Wait for queue space
        }
        
        if (!sent) {
            log(LOG_WARNING, false, "  ✗ Failed to send ADC[%d] config after retries\n", i);
        }
        
        delay(10);  // Delay between messages
    }
    
    // ========================================================================
    // Push DAC Output configurations (indices 8-9)
    // ========================================================================
    for (int i = 0; i < MAX_DAC_OUTPUTS; i++) {
        if (!ioConfig.dacOutputs[i].enabled) continue;
        
        IPC_ConfigAnalogOutput_t cfg;
        cfg.index = 8 + i;
        strncpy(cfg.unit, ioConfig.dacOutputs[i].unit, sizeof(cfg.unit) - 1);
        cfg.unit[sizeof(cfg.unit) - 1] = '\0';
        cfg.calScale = ioConfig.dacOutputs[i].cal.scale;
        cfg.calOffset = ioConfig.dacOutputs[i].cal.offset;
        
        // Retry up to 10 times if queue is full
        bool sent = false;
        for (int retry = 0; retry < 10; retry++) {
            if (ipc.sendPacket(IPC_MSG_CONFIG_ANALOG_OUTPUT, (uint8_t*)&cfg, sizeof(cfg))) {
                sent = true;
                sentCount++;
                log(LOG_DEBUG, false, "  → DAC[%d]: %s, scale=%.3f, offset=%.3f\n",
                    8 + i, cfg.unit, cfg.calScale, cfg.calOffset);
                break;
            }
            ipc.update();  // Process any pending RX/TX
            delay(10);  // Wait for queue space
        }
        
        if (!sent) {
            log(LOG_WARNING, false, "  ✗ Failed to send DAC[%d] config after retries\n", 8 + i);
        }
        
        delay(10);
    }
    
    // ========================================================================
    // Push RTD Sensor configurations (indices 10-12)
    // ========================================================================
    for (int i = 0; i < MAX_RTD_SENSORS; i++) {
        if (!ioConfig.rtdSensors[i].enabled) continue;
        
        IPC_ConfigRTD_t cfg;
        cfg.index = 10 + i;
        strncpy(cfg.unit, ioConfig.rtdSensors[i].unit, sizeof(cfg.unit) - 1);
        cfg.unit[sizeof(cfg.unit) - 1] = '\0';
        cfg.calScale = ioConfig.rtdSensors[i].cal.scale;
        cfg.calOffset = ioConfig.rtdSensors[i].cal.offset;
        cfg.wireConfig = ioConfig.rtdSensors[i].wireConfig;
        cfg.nominalOhms = ioConfig.rtdSensors[i].nominalOhms;
        
        // Retry up to 10 times if queue is full
        bool sent = false;
        for (int retry = 0; retry < 10; retry++) {
            if (ipc.sendPacket(IPC_MSG_CONFIG_RTD, (uint8_t*)&cfg, sizeof(cfg))) {
                sent = true;
                sentCount++;
                log(LOG_DEBUG, false, "  → RTD[%d]: %s, %d-wire, PT%d, scale=%.3f, offset=%.3f\n",
                    10 + i, cfg.unit, cfg.wireConfig, cfg.nominalOhms, cfg.calScale, cfg.calOffset);
                break;
            }
            ipc.update();  // Process any pending RX/TX
            delay(10);  // Wait for queue space
        }
        
        if (!sent) {
            log(LOG_WARNING, false, "  ✗ Failed to send RTD[%d] config after retries\n", 10 + i);
        }
        
        delay(10);
    }
    
    // ========================================================================
    // Push GPIO Input configurations (indices 13-20)
    // ========================================================================
    for (int i = 0; i < MAX_GPIO; i++) {
        if (!ioConfig.gpio[i].enabled) continue;
        
        IPC_ConfigGPIO_t cfg;
        cfg.index = 13 + i;
        strncpy(cfg.name, ioConfig.gpio[i].name, sizeof(cfg.name) - 1);
        cfg.name[sizeof(cfg.name) - 1] = '\0';
        cfg.pullMode = (uint8_t)ioConfig.gpio[i].pullMode;
        cfg.enabled = ioConfig.gpio[i].enabled;
        
        // Retry up to 10 times if queue is full
        bool sent = false;
        for (int retry = 0; retry < 10; retry++) {
            if (ipc.sendPacket(IPC_MSG_CONFIG_GPIO, (uint8_t*)&cfg, sizeof(cfg))) {
                sent = true;
                sentCount++;
                const char* pullStr = (cfg.pullMode == 1) ? "PULL-UP" :
                                      (cfg.pullMode == 2) ? "PULL-DOWN" : "HIGH-Z";
                log(LOG_DEBUG, false, "  → GPIO[%d]: %s, pull=%s\n",
                    13 + i, cfg.name, pullStr);
                break;
            }
            ipc.update();  // Process any pending RX/TX
            delay(10);  // Wait for queue space
        }
        
        if (!sent) {
            log(LOG_WARNING, false, "  ✗ Failed to send GPIO[%d] config after retries\n", 13 + i);
        }
        
        delay(10);
    }
    
    // ========================================================================
    // Push Digital Output configurations (indices 21-25)
    // ========================================================================
    for (int i = 0; i < MAX_DIGITAL_OUTPUTS; i++) {
        if (!ioConfig.digitalOutputs[i].enabled) continue;
        
        IPC_ConfigDigitalOutput_t cfg;
        cfg.index = 21 + i;
        strncpy(cfg.name, ioConfig.digitalOutputs[i].name, sizeof(cfg.name) - 1);
        cfg.name[sizeof(cfg.name) - 1] = '\0';
        cfg.mode = (uint8_t)ioConfig.digitalOutputs[i].mode;
        cfg.enabled = ioConfig.digitalOutputs[i].enabled;
        
        // Retry up to 10 times if queue is full
        bool sent = false;
        for (int retry = 0; retry < 10; retry++) {
            if (ipc.sendPacket(IPC_MSG_CONFIG_DIGITAL_OUTPUT, (uint8_t*)&cfg, sizeof(cfg))) {
                sent = true;
                sentCount++;
                const char* modeStr = (cfg.mode == 1) ? "PWM" : "ON/OFF";
                log(LOG_DEBUG, false, "  → DigitalOutput[%d]: %s, mode=%s\n",
                    21 + i, cfg.name, modeStr);
                break;
            }
            ipc.update();
            delay(10);
        }
        
        if (!sent) {
            log(LOG_WARNING, false, "  ✗ Failed to send DigitalOutput[%d] config after retries\n", 21 + i);
        }
        
        delay(10);
    }
    
    // ========================================================================
    // Push Stepper Motor configuration (index 26)
    // ========================================================================
    if (ioConfig.stepperMotor.enabled) {
        IPC_ConfigStepper_t cfg;
        cfg.index = 26;
        strncpy(cfg.name, ioConfig.stepperMotor.name, sizeof(cfg.name) - 1);
        cfg.name[sizeof(cfg.name) - 1] = '\0';
        cfg.stepsPerRev = ioConfig.stepperMotor.stepsPerRev;
        cfg.maxRPM = ioConfig.stepperMotor.maxRPM;
        cfg.holdCurrent_mA = ioConfig.stepperMotor.holdCurrent_mA;
        cfg.runCurrent_mA = ioConfig.stepperMotor.runCurrent_mA;
        cfg.acceleration = ioConfig.stepperMotor.acceleration;
        cfg.invertDirection = ioConfig.stepperMotor.invertDirection;
        cfg.enabled = ioConfig.stepperMotor.enabled;
        
        // Retry up to 10 times if queue is full
        bool sent = false;
        for (int retry = 0; retry < 10; retry++) {
            if (ipc.sendPacket(IPC_MSG_CONFIG_STEPPER, (uint8_t*)&cfg, sizeof(cfg))) {
                sent = true;
                sentCount++;
                log(LOG_DEBUG, false, "  → Stepper[26]: %s, maxRPM=%d, steps=%d, Irun=%dmA\n",
                    cfg.name, cfg.maxRPM, cfg.stepsPerRev, cfg.runCurrent_mA);
                break;
            }
            ipc.update();
            delay(10);
        }
        
        if (!sent) {
            log(LOG_WARNING, false, "  ✗ Failed to send Stepper config after retries\n");
        }
        
        delay(10);
    }
    
    // ========================================================================
    // Push DC Motor configurations (indices 27-30)
    // ========================================================================
    for (int i = 0; i < MAX_DC_MOTORS; i++) {
        if (!ioConfig.dcMotors[i].enabled) continue;
        
        IPC_ConfigDCMotor_t cfg;
        cfg.index = 27 + i;
        strncpy(cfg.name, ioConfig.dcMotors[i].name, sizeof(cfg.name) - 1);
        cfg.name[sizeof(cfg.name) - 1] = '\0';
        cfg.invertDirection = ioConfig.dcMotors[i].invertDirection;
        cfg.enabled = ioConfig.dcMotors[i].enabled;
        
        // Retry up to 10 times if queue is full
        bool sent = false;
        for (int retry = 0; retry < 10; retry++) {
            if (ipc.sendPacket(IPC_MSG_CONFIG_DCMOTOR, (uint8_t*)&cfg, sizeof(cfg))) {
                sent = true;
                sentCount++;
                log(LOG_DEBUG, false, "  → DCMotor[%d]: %s, invert=%s\n",
                    27 + i, cfg.name, cfg.invertDirection ? "YES" : "NO");
                break;
            }
            ipc.update();
            delay(10);
        }
        
        if (!sent) {
            log(LOG_WARNING, false, "  ✗ Failed to send DCMotor[%d] config after retries\n", 27 + i);
        }
        
        delay(10);
    }
    
    // ========================================================================
    // Push COM Port configurations (indices 0-3)
    // ========================================================================
    for (int i = 0; i < MAX_COM_PORTS; i++) {
        if (!ioConfig.comPorts[i].enabled) continue;
        
        IPC_ConfigComPort_t cfg;
        cfg.index = i;
        cfg.baudRate = ioConfig.comPorts[i].baudRate;
        cfg.dataBits = ioConfig.comPorts[i].dataBits;
        cfg.stopBits = ioConfig.comPorts[i].stopBits;
        cfg.parity = ioConfig.comPorts[i].parity;
        
        // Retry up to 10 times if queue is full
        bool sent = false;
        for (int retry = 0; retry < 10; retry++) {
            if (ipc.sendPacket(IPC_MSG_CONFIG_COMPORT, (uint8_t*)&cfg, sizeof(cfg))) {
                sent = true;
                sentCount++;
                const char* parityStr = (cfg.parity == 0) ? "N" :
                                       (cfg.parity == 1) ? "O" : "E";
                log(LOG_DEBUG, false, "  → COM Port[%d]: %lu baud, %d%s%.0f\n",
                    i, cfg.baudRate, cfg.dataBits, parityStr, cfg.stopBits);
                break;
            }
            ipc.update();
            delay(10);
        }
        
        if (!sent) {
            log(LOG_WARNING, false, "  ✗ Failed to send COM Port[%d] config after retries\n", i);
        }
        
        delay(10);
    }
    
    // ========================================================================
    // Push Dynamic Device configurations (indices 60-79)
    // ========================================================================
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (!ioConfig.devices[i].isActive) continue;
        
        uint8_t dynamicIndex = ioConfig.devices[i].dynamicIndex;
        
        // Convert to IPC config
        IPC_DeviceConfig_t ipcConfig;
        memset(&ipcConfig, 0, sizeof(IPC_DeviceConfig_t));
        
        // Map driver type to IPC device type
        switch (ioConfig.devices[i].driverType) {
            case DEVICE_DRIVER_HAMILTON_PH:  ipcConfig.deviceType = IPC_DEV_HAMILTON_PH; break;
            case DEVICE_DRIVER_HAMILTON_DO:  ipcConfig.deviceType = IPC_DEV_HAMILTON_DO; break;
            case DEVICE_DRIVER_HAMILTON_OD:  ipcConfig.deviceType = IPC_DEV_HAMILTON_OD; break;
            case DEVICE_DRIVER_ALICAT_MFC:   ipcConfig.deviceType = IPC_DEV_ALICAT_MFC; break;
            case DEVICE_DRIVER_PRESSURE_CONTROLLER: ipcConfig.deviceType = IPC_DEV_PRESSURE_CTRL; break;
            default: ipcConfig.deviceType = IPC_DEV_NONE; break;
        }
        
        // Map interface type to IPC bus type
        switch (ioConfig.devices[i].interfaceType) {
            case DEVICE_INTERFACE_MODBUS_RTU:
                ipcConfig.busType = IPC_BUS_MODBUS_RTU;
                ipcConfig.busIndex = ioConfig.devices[i].modbus.portIndex;
                ipcConfig.address = ioConfig.devices[i].modbus.slaveID;
                break;
                
            case DEVICE_INTERFACE_ANALOGUE_IO:
                ipcConfig.busType = IPC_BUS_ANALOG;
                ipcConfig.busIndex = ioConfig.devices[i].analogueIO.dacOutputIndex;
                ipcConfig.address = 0;
                break;
                
            case DEVICE_INTERFACE_MOTOR_DRIVEN:
                ipcConfig.busType = IPC_BUS_DIGITAL;
                ipcConfig.busIndex = ioConfig.devices[i].motorDriven.motorIndex;
                ipcConfig.address = 0;
                break;
                
            default:
                ipcConfig.busType = IPC_BUS_NONE;
                break;
        }
        
        // Send device create command
        IPC_DeviceCreate_t createCmd;
        createCmd.startIndex = dynamicIndex;
        memcpy(&createCmd.config, &ipcConfig, sizeof(IPC_DeviceConfig_t));
        
        // Retry up to 10 times if queue is full
        bool sent = false;
        for (int retry = 0; retry < 10; retry++) {
            if (ipc.sendPacket(IPC_MSG_DEVICE_CREATE, (uint8_t*)&createCmd, sizeof(createCmd))) {
                sent = true;
                sentCount++;
                const char* devTypeStr = (ipcConfig.deviceType == IPC_DEV_HAMILTON_PH) ? "Hamilton pH" :
                                       (ipcConfig.deviceType == IPC_DEV_HAMILTON_DO) ? "Hamilton DO" :
                                       (ipcConfig.deviceType == IPC_DEV_HAMILTON_OD) ? "Hamilton OD" :
                                       (ipcConfig.deviceType == IPC_DEV_ALICAT_MFC) ? "Alicat MFC" : "Unknown";
                log(LOG_INFO, false, "  → Device[%d]: %s, type=%s, bus=%d, addr=%d\n",
                    dynamicIndex, ioConfig.devices[i].name, devTypeStr,
                    ipcConfig.busIndex, ipcConfig.address);
                break;
            }
            ipc.update();
            delay(10);
        }
        
        if (!sent) {
            log(LOG_WARNING, false, "  ✗ Failed to send Device[%d] config after retries\n", dynamicIndex);
        }
        
        delay(20);  // Extra delay for device initialization
    }
    
    // Push pressure controller calibration (after device creation)
    log(LOG_INFO, true, "Sending pressure controller calibration...\n");
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (ioConfig.devices[i].isActive && 
            ioConfig.devices[i].driverType == (DeviceDriverType)IPC_DEV_PRESSURE_CTRL &&
            ioConfig.devices[i].interfaceType == DEVICE_INTERFACE_ANALOGUE_IO) {
            
            IPC_ConfigPressureCtrl_t cfg;
            cfg.controlIndex = ioConfig.devices[i].dynamicIndex - 20;  // Control index
            cfg.dacIndex = ioConfig.devices[i].analogueIO.dacOutputIndex;
            strncpy(cfg.unit, ioConfig.devices[i].analogueIO.unit, sizeof(cfg.unit) - 1);
            cfg.unit[sizeof(cfg.unit) - 1] = '\0';
            cfg.scale = ioConfig.devices[i].analogueIO.scale;
            cfg.offset = ioConfig.devices[i].analogueIO.offset;
            
            bool sent = false;
            for (int retry = 0; retry < 10; retry++) {
                if (ipc.sendPacket(IPC_MSG_CONFIG_PRESSURE_CTRL, (uint8_t*)&cfg, sizeof(cfg))) {
                    sent = true;
                    sentCount++;
                    log(LOG_DEBUG, false, "  → Pressure[%d]: scale=%.6f, offset=%.2f %s at DAC %d\n",
                        cfg.controlIndex, cfg.scale, cfg.offset, 
                        cfg.unit, cfg.dacIndex);
                    break;
                }
                ipc.update();
                delay(10);
            }
            
            if (!sent) {
                log(LOG_WARNING, false, "  ✗ Failed to send pressure controller calibration\n");
            }
            
            delay(10);
        }
    }
    
    // ========================================================================
    // Push Temperature Controller configurations (indices 40-42)
    // ========================================================================
    for (int i = 0; i < MAX_TEMP_CONTROLLERS; i++) {
        if (!ioConfig.tempControllers[i].isActive) continue;
        
        IPC_ConfigTempController_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        
        cfg.index = 40 + i;
        cfg.isActive = true;
        strncpy(cfg.name, ioConfig.tempControllers[i].name, sizeof(cfg.name) - 1);
        cfg.enabled = ioConfig.tempControllers[i].enabled;
        cfg.pvSourceIndex = ioConfig.tempControllers[i].pvSourceIndex;
        cfg.outputIndex = ioConfig.tempControllers[i].outputIndex;
        cfg.controlMethod = (uint8_t)ioConfig.tempControllers[i].controlMethod;
        cfg.setpoint = ioConfig.tempControllers[i].setpoint;
        cfg.hysteresis = ioConfig.tempControllers[i].hysteresis;
        cfg.kP = ioConfig.tempControllers[i].kP;
        cfg.kI = ioConfig.tempControllers[i].kI;
        cfg.kD = ioConfig.tempControllers[i].kD;
        cfg.integralWindup = ioConfig.tempControllers[i].integralWindup;
        cfg.outputMin = ioConfig.tempControllers[i].outputMin;
        cfg.outputMax = ioConfig.tempControllers[i].outputMax;
        
        // Retry up to 10 times if queue is full
        bool sent = false;
        for (int retry = 0; retry < 10; retry++) {
            if (ipc.sendPacket(IPC_MSG_CONFIG_TEMP_CONTROLLER, (uint8_t*)&cfg, sizeof(cfg))) {
                sent = true;
                sentCount++;
                log(LOG_INFO, false, "  → TempController[%d]: %s, sensor=%d, output=%d, method=%s\n",
                    cfg.index, cfg.name, cfg.pvSourceIndex, cfg.outputIndex,
                    cfg.controlMethod == 0 ? "On/Off" : "PID");
                break;
            }
            ipc.update();
            delay(10);
        }
        
        if (!sent) {
            log(LOG_WARNING, false, "  ✗ Failed to send TempController[%d] config after retries\n", 40 + i);
        }
        
        delay(10);
    }
    
    log(LOG_INFO, false, "IO configuration push complete: %d objects configured (inputs + outputs + COM ports + devices + controllers)\n", sentCount);
}

// ============================================================================
// Device Management Helper Functions
// ============================================================================

/**
 * @brief Get the number of object indices needed for a device type
 * @param driverType Device driver type
 * @return Number of consecutive indices needed (1-4)
 */
static uint8_t getDeviceObjectCount(DeviceDriverType driverType) {
    switch(driverType) {
        case DEVICE_DRIVER_HAMILTON_PH:   // pH + Temperature
        case DEVICE_DRIVER_HAMILTON_DO:   // DO + Temperature  
        case DEVICE_DRIVER_HAMILTON_OD:   // OD + Temperature
        case DEVICE_DRIVER_ALICAT_MFC:    // Flow + Pressure
            return 2;
        
        case DEVICE_DRIVER_PRESSURE_CONTROLLER:  // Control only (no sensor)
            return 0;  // No sensor objects - only control object
        
        // Future multi-sensor devices
        // case DEVICE_DRIVER_BME280:     // Temp + Humidity + Pressure
        // case DEVICE_DRIVER_SCD40:      // CO2 + Temp + Humidity
        //     return 3;
        
        default:
            return 1;  // Default to single index
    }
}

/**
 * @brief Check if a range of indices is available (not in use by any device)
 */
static bool isIndexRangeAvailable(uint8_t startIndex, uint8_t count) {
    if (count == 0) return false;
    
    // Validate range
    if (startIndex < DYNAMIC_INDEX_START || startIndex > DYNAMIC_INDEX_END) {
        return false;
    }
    
    // Check each index in the range
    for (uint8_t offset = 0; offset < count; offset++) {
        uint8_t idx = startIndex + offset;
        
        // Check if idx exceeds valid range
        if (idx > DYNAMIC_INDEX_END) {
            return false;
        }
        
        // Check if this sensor index is used by any device
        for (int i = 0; i < MAX_DEVICES; i++) {
            if (!ioConfig.devices[i].isActive) continue;
            
            // Get the object count for this device to know its index range
            uint8_t devObjCount = getDeviceObjectCount(ioConfig.devices[i].driverType);
            
            // All devices need at least 1 slot (same logic as allocateDynamicIndex)
            if (devObjCount == 0) {
                devObjCount = 1;
            }
            
            uint8_t devStartIdx = ioConfig.devices[i].dynamicIndex;
            uint8_t devEndIdx = devStartIdx + devObjCount - 1;
            
            // Check if idx falls within this device's sensor range
            if (idx >= devStartIdx && idx <= devEndIdx) {
                return false;  // Sensor index is in use
            }
        }
    }
    
    return true;  // All indices in range are available
}

/**
 * @brief Allocate consecutive dynamic indices for a device
 * 
 * Simplified architecture: ALL devices allocate from 70-99 range (sensor index)
 * - Control index is automatically: dynamicIndex - 20 → 50-69
 * - Control-only devices get objectCount=0, but still allocate 1 sensor slot
 * - Sensor slot can store actual/feedback value for diagnostics
 * 
 * @param driverType Device driver type (to determine how many indices needed)
 * @return Starting dynamic index (70-99), or -1 if not enough consecutive slots available
 */
int8_t allocateDynamicIndex(DeviceDriverType driverType) {
    uint8_t objectCount = getDeviceObjectCount(driverType);
    
    // All devices need at least 1 slot (for control-only devices, this holds feedback/actual value)
    if (objectCount == 0) {
        objectCount = 1;  // Allocate 1 sensor slot for feedback/diagnostics
    }
    
    // Scan through dynamic index range (70-99) to find consecutive free indices
    for (uint8_t idx = DYNAMIC_INDEX_START; idx <= DYNAMIC_INDEX_END; idx++) {
        // Check if we have enough room (don't go past DYNAMIC_INDEX_END)
        if (idx + objectCount - 1 > DYNAMIC_INDEX_END) {
            break;  // Not enough room at the end
        }
        
        // Check if this range is available
        if (isIndexRangeAvailable(idx, objectCount)) {
            uint8_t controlIdx = idx - 20;  // Calculate paired control index
            log(LOG_DEBUG, false, "Allocated device indices: sensor=%d, control=%d (count=%d)\n", 
                idx, controlIdx, objectCount);
            return idx;  // Found available range
        }
    }
    
    log(LOG_WARNING, false, "No %d consecutive indices available in range 70-99\n", objectCount);
    return -1;  // No consecutive range available
}

/**
 * @brief Legacy function for backward compatibility - allocates single index
 * @deprecated Use allocateDynamicIndex(DeviceDriverType) instead
 * @return Dynamic index (60-79), or -1 if all slots are full
 */
int8_t allocateDynamicIndex() {
    // Scan through dynamic index range (60-79) to find the first unused index
    for (uint8_t idx = DYNAMIC_INDEX_START; idx <= DYNAMIC_INDEX_END; idx++) {
        if (isIndexRangeAvailable(idx, 1)) {
            return idx;
        }
    }
    
    return -1;  // All dynamic indices are in use
}

/**
 * @brief Free a dynamic index when device is deleted
 * @param index Dynamic index to free (60-79)
 */
void freeDynamicIndex(uint8_t index) {
    if (index < DYNAMIC_INDEX_START || index > DYNAMIC_INDEX_END) {
        log(LOG_WARNING, true, "Attempted to free invalid dynamic index: %d\n", index);
        return;
    }
    
    // Find the device using this index and mark it as inactive
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (ioConfig.devices[i].isActive && ioConfig.devices[i].dynamicIndex == index) {
            ioConfig.devices[i].isActive = false;
            ioConfig.devices[i].dynamicIndex = 0xFF;
            log(LOG_INFO, true, "Freed dynamic index %d\n", index);
            return;
        }
    }
    
    log(LOG_WARNING, true, "Dynamic index %d not found in active devices\n", index);
}

/**
 * @brief Check if a dynamic index is currently in use
 * @param index Dynamic index to check (60-79)
 * @return true if index is in use, false otherwise
 */
bool isDynamicIndexInUse(uint8_t index) {
    if (index < DYNAMIC_INDEX_START || index > DYNAMIC_INDEX_END) {
        return false;
    }
    
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (ioConfig.devices[i].isActive && ioConfig.devices[i].dynamicIndex == index) {
            return true;
        }
    }
    
    return false;
}

/**
 * @brief Find device array position by dynamic index
 * @param dynamicIndex Dynamic index to search for (60-79)
 * @return Device array position (0-19), or -1 if not found
 */
int8_t findDeviceByIndex(uint8_t dynamicIndex) {
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (ioConfig.devices[i].isActive && ioConfig.devices[i].dynamicIndex == dynamicIndex) {
            return i;
        }
    }
    
    return -1;  // Device not found
}

/**
 * @brief Get the control object index for a device
 * 
 * Simplified architecture: ALL devices follow same pattern:
 * - Sensor index (dynamicIndex): 70-99
 * - Control index: dynamicIndex - 20 → 50-69
 * 
 * This provides consistent, predictable index mapping with no special cases.
 * 
 * @param device Pointer to device configuration
 * @return Control object index (50-69), or 0xFF if device is NULL
 */
uint8_t getDeviceControlIndex(const DeviceConfig* device) {
    if (device == nullptr) {
        log(LOG_ERROR, false, "getDeviceControlIndex: NULL device pointer\n");
        return 0xFF;  // Invalid index
    }
    
    // All devices: control index = sensor index - 20
    // Maps 70-99 → 50-69
    return device->dynamicIndex - 20;
}
