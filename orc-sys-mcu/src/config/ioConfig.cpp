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
}

/**
 * @brief Load IO configuration from LittleFS
 * @return true if successful, false if file not found or invalid
 */
bool loadIOConfig() {
    log(LOG_INFO, true, "Loading IO configuration from %s\n", IO_CONFIG_FILENAME);
    
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
    DynamicJsonDocument doc(8192);
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
    DynamicJsonDocument doc(8192);
    
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
    
    log(LOG_INFO, false, "IO configuration push complete: %d objects configured (inputs + outputs)\n", sentCount);
}
