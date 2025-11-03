#include "controller_manager.h"
#include "ctrl_ph.h"
#include "../sys_init.h"
#include "../drivers/onboard/drv_output.h"

// External references
extern TaskScheduler tasks;
extern ObjectIndex_t objIndex[];
extern int numObjects;

// Static member initialization
ManagedController ControllerManager::controllers[MAX_TEMP_CONTROLLERS];
ManagedpHController ControllerManager::phController;
bool ControllerManager::initialized = false;

// ============================================================================
// Static Controller Task Wrappers (for scheduler - can't use capturing lambdas)
// ============================================================================

// NOTE: The scheduler's TaskCallback is a plain function pointer that cannot
// capture variables. We need 3 separate non-capturing wrapper functions
// because C++ doesn't allow parameterized function pointers.

// Store controller instances for each wrapper slot (0-2 maps to controllers 40-42)
static TemperatureController* controllerInstances[MAX_TEMP_CONTROLLERS] = {nullptr};

// Generic task wrapper functions - one per slot
static void controllerTaskWrapper_0() {
    if (controllerInstances[0] != nullptr) {
        controllerInstances[0]->update();
    }
}

static void controllerTaskWrapper_1() {
    if (controllerInstances[1] != nullptr) {
        controllerInstances[1]->update();
    }
}

static void controllerTaskWrapper_2() {
    if (controllerInstances[2] != nullptr) {
        controllerInstances[2]->update();
    }
}

// Array of task wrapper function pointers (indexed by slot 0-2)
typedef void (*TaskWrapperFunc)();
static TaskWrapperFunc taskWrappers[MAX_TEMP_CONTROLLERS] = {
    controllerTaskWrapper_0,
    controllerTaskWrapper_1,
    controllerTaskWrapper_2
};

// ============================================================================
// INITIALIZATION
// ============================================================================

bool ControllerManager::init() {
    if (initialized) {
        return true;  // Already initialized
    }
    
    // Initialize all controller slots
    for (int i = 0; i < MAX_TEMP_CONTROLLERS; i++) {
        controllers[i].index = 40 + i;
        controllers[i].controllerInstance = nullptr;
        controllers[i].controlObject = nullptr;
        controllers[i].updateTask = nullptr;
        controllers[i].active = false;
        controllers[i].message[0] = '\0';
    }
    
    Serial.println("[CTRL MGR] Controller Manager initialized");
    initialized = true;
    return true;
}

// ============================================================================
// LIFECYCLE MANAGEMENT
// ============================================================================

bool ControllerManager::createController(uint8_t index, const IPC_ConfigTempController_t* config) {
    if (!initialized) {
        Serial.println("[CTRL MGR] ERROR: Not initialized");
        return false;
    }
    
    // Validate index
    int arrIdx = indexToArrayIndex(index);
    if (arrIdx < 0) {
        Serial.printf("[CTRL MGR] ERROR: Invalid index %d\n", index);
        return false;
    }
    
    // Validate configuration
    if (!validateConfig(config)) {
        Serial.println("[CTRL MGR] ERROR: Invalid configuration");
        return false;
    }
    
    ManagedController* ctrl = &controllers[arrIdx];
    
    // If controller already exists, delete it first
    if (ctrl->active) {
        Serial.printf("[CTRL MGR] Controller %d already exists, deleting first\n", index);
        deleteController(index);
    }
    
    // Create TemperatureControl_t object (will be registered in objIndex)
    ctrl->controlObject = new TemperatureControl_t();
    if (ctrl->controlObject == nullptr) {
        Serial.println("[CTRL MGR] ERROR: Failed to allocate control object");
        return false;
    }
    
    // Initialize control object from config
    ctrl->controlObject->sensorIndex = config->pvSourceIndex;
    ctrl->controlObject->outputIndex = config->outputIndex;
    ctrl->controlObject->enabled = config->enabled;
    ctrl->controlObject->autotuning = false;
    ctrl->controlObject->controlMethod = config->controlMethod;
    ctrl->controlObject->hysteresis = config->hysteresis;
    ctrl->controlObject->setpoint = config->setpoint;
    ctrl->controlObject->setpointMin = 0.0f;  // TODO: Add to config if needed
    ctrl->controlObject->setpointMax = 200.0f;  // TODO: Add to config if needed
    ctrl->controlObject->kp = config->kP;
    ctrl->controlObject->ki = config->kI;
    ctrl->controlObject->kd = config->kD;
    ctrl->controlObject->outputMin = config->outputMin;
    ctrl->controlObject->outputMax = config->outputMax;
    ctrl->controlObject->outputInverted = false;  // TODO: Add to config if needed
    ctrl->controlObject->currentTemp = 0.0f;
    ctrl->controlObject->currentOutput = 0.0f;
    ctrl->controlObject->processError = 0.0f;
    ctrl->controlObject->fault = false;
    ctrl->controlObject->newMessage = false;
    strcpy(ctrl->controlObject->message, "Controller initialized");
    
    // Create TemperatureController instance
    ctrl->controllerInstance = new TemperatureController();
    if (ctrl->controllerInstance == nullptr) {
        Serial.println("[CTRL MGR] ERROR: Failed to allocate controller instance");
        delete ctrl->controlObject;
        ctrl->controlObject = nullptr;
        return false;
    }
    
    // Initialize controller with control object
    if (!ctrl->controllerInstance->begin(ctrl->controlObject)) {
        Serial.println("[CTRL MGR] ERROR: Failed to initialize controller");
        delete ctrl->controllerInstance;
        delete ctrl->controlObject;
        ctrl->controllerInstance = nullptr;
        ctrl->controlObject = nullptr;
        return false;
    }
    
    // Assign sensor and output
    if (!ctrl->controllerInstance->assignSensor(config->pvSourceIndex)) {
        Serial.printf("[CTRL MGR] ERROR: Failed to assign sensor %d\n", config->pvSourceIndex);
        delete ctrl->controllerInstance;
        delete ctrl->controlObject;
        ctrl->controllerInstance = nullptr;
        ctrl->controlObject = nullptr;
        return false;
    }
    
    if (!ctrl->controllerInstance->assignOutput(config->outputIndex)) {
        Serial.printf("[CTRL MGR] ERROR: Failed to assign output %d\n", config->outputIndex);
        delete ctrl->controllerInstance;
        delete ctrl->controlObject;
        ctrl->controllerInstance = nullptr;
        ctrl->controlObject = nullptr;
        return false;
    }
    
    // Set PID parameters
    ctrl->controllerInstance->setPIDGains(config->kP, config->kI, config->kD);
    ctrl->controllerInstance->setOutputLimits(config->outputMin, config->outputMax);
    ctrl->controllerInstance->setSetpoint(config->setpoint);
    
    // Enable if requested
    if (config->enabled) {
        ctrl->controllerInstance->enable();
    }
    
    // Register in object index
    registerControllerObject(ctrl);
    
    // Add scheduler task (100ms = 10Hz update rate)
    ctrl->updateTask = addControllerTask(ctrl);
    if (ctrl->updateTask == nullptr) {
        Serial.println("[CTRL MGR] WARNING: Failed to add scheduler task");
        // Continue anyway - controller can still be used manually
    }
    
    // Mark as active
    ctrl->active = true;
    strcpy(ctrl->message, "Controller created");
    
    Serial.printf("[CTRL MGR] ✓ Created controller %d: sensor=%d, output=%d, method=%s\n",
                 index, config->pvSourceIndex, config->outputIndex,
                 config->controlMethod == 0 ? "On/Off" : "PID");
    
    return true;
}

bool ControllerManager::deleteController(uint8_t index) {
    if (!initialized) {
        return false;
    }
    
    int arrIdx = indexToArrayIndex(index);
    if (arrIdx < 0) {
        return false;
    }
    
    ManagedController* ctrl = &controllers[arrIdx];
    
    if (!ctrl->active) {
        return true;  // Already deleted
    }
    
    // Stop update task
    if (ctrl->updateTask != nullptr) {
        removeControllerTask(ctrl->updateTask);
        ctrl->updateTask = nullptr;
    }
    
    // Disable controller before deletion
    if (ctrl->controllerInstance != nullptr) {
        ctrl->controllerInstance->disable();
        delete ctrl->controllerInstance;
        ctrl->controllerInstance = nullptr;
    }
    
    // Clear static array entry
    clearControllerInstance(index);
    
    // Unregister from object index
    unregisterControllerObject(ctrl);
    
    // Delete control object
    if (ctrl->controlObject != nullptr) {
        delete ctrl->controlObject;
        ctrl->controlObject = nullptr;
    }
    
    // Mark as inactive
    ctrl->active = false;
    strcpy(ctrl->message, "Controller deleted");
    
    Serial.printf("[CTRL MGR] ✓ Deleted controller %d\n", index);
    
    return true;
}

bool ControllerManager::configureController(uint8_t index, const IPC_ConfigTempController_t* config) {
    // For now, just recreate the controller
    // Future optimization: update in-place for parameter changes
    return createController(index, config);
}

// ============================================================================
// CONTROL COMMANDS
// ============================================================================

bool ControllerManager::setSetpoint(uint8_t index, float setpoint) {
    ManagedController* ctrl = findController(index);
    if (ctrl == nullptr || !ctrl->active) {
        return false;
    }
    
    if (ctrl->controllerInstance == nullptr) {
        return false;
    }
    
    return ctrl->controllerInstance->setSetpoint(setpoint);
}

bool ControllerManager::enableController(uint8_t index) {
    ManagedController* ctrl = findController(index);
    if (ctrl == nullptr || !ctrl->active) {
        return false;
    }
    
    if (ctrl->controllerInstance == nullptr) {
        return false;
    }
    
    // Force output to correct mode based on control method
    // This ensures the output is in the right mode even if user changed it manually
    uint8_t outputIndex = ctrl->controlObject->outputIndex;
    if (outputIndex >= 21 && outputIndex <= 25) {
        DigitalOutput_t* output = (DigitalOutput_t*)objIndex[outputIndex].obj;
        
        if (ctrl->controlObject->controlMethod == 0) {
            // ON/OFF mode - ensure output is in digital mode (PWM disabled)
            if (output->pwmEnabled) {
                output->pwmEnabled = false;
                output->state = false;
                output_force_digital_mode(outputIndex);
                Serial.printf("[CTRL MGR] Forced output %d to ON/OFF mode for controller %d\n", 
                             outputIndex, index);
            }
        } else {
            // PID mode - ensure output is in PWM mode
            if (!output->pwmEnabled) {
                output->pwmEnabled = true;
                output->pwmDuty = 0.0f;
                // output_update() will handle the actual pin configuration
                Serial.printf("[CTRL MGR] Set output %d to PWM mode for controller %d\n", 
                             outputIndex, index);
            }
        }
    }
    
    ctrl->controllerInstance->enable();
    return true;
}

bool ControllerManager::disableController(uint8_t index) {
    ManagedController* ctrl = findController(index);
    if (ctrl == nullptr || !ctrl->active) {
        return false;
    }
    
    if (ctrl->controllerInstance == nullptr) {
        return false;
    }
    
    ctrl->controllerInstance->disable();
    return true;
}

bool ControllerManager::startAutotune(uint8_t index, float targetSetpoint, float outputStep) {
    ManagedController* ctrl = findController(index);
    if (ctrl == nullptr || !ctrl->active) {
        return false;
    }
    
    if (ctrl->controllerInstance == nullptr) {
        return false;
    }
    
    return ctrl->controllerInstance->startAutotune(targetSetpoint, outputStep);
}

bool ControllerManager::stopAutotune(uint8_t index) {
    ManagedController* ctrl = findController(index);
    if (ctrl == nullptr || !ctrl->active) {
        return false;
    }
    
    if (ctrl->controllerInstance == nullptr) {
        return false;
    }
    
    ctrl->controllerInstance->stopAutotune();
    return true;
}

// ============================================================================
// pH CONTROLLER FUNCTIONS (Index 43)
// ============================================================================

bool ControllerManager::createpHController(const IPC_ConfigpHController_t* config) {
    if (config->index != 43) {
        Serial.printf("[CTRL MGR] pH controller must use index 43\n");
        return false;
    }
    
    // Delete existing controller if present
    if (phController.active) {
        deletepHController();
    }
    
    // Allocate control object
    pHControl_t* control = new pHControl_t();
    if (control == nullptr) {
        Serial.println("[CTRL MGR] Failed to allocate pH control object");
        return false;
    }
    
    // Initialize control object
    memset(control, 0, sizeof(pHControl_t));
    control->sensorIndex = config->pvSourceIndex;
    control->enabled = config->enabled;
    control->setpoint = config->setpoint;
    control->deadband = config->deadband;
    
    // Acid dosing configuration
    control->acidEnabled = config->acidEnabled;
    control->acidOutputType = config->acidOutputType;
    control->acidOutputIndex = config->acidOutputIndex;
    control->acidMotorPower = config->acidMotorPower;
    control->acidDosingTime_ms = config->acidDosingTime_ms;
    control->acidDosingInterval_ms = config->acidDosingInterval_ms;
    control->acidVolumePerDose_mL = config->acidVolumePerDose_mL;
    control->acidCumulativeVolume_mL = 0.0f;  // Initialize to zero
    control->lastAcidDoseTime = 0;
    
    // Alkaline dosing configuration
    control->alkalineEnabled = config->alkalineEnabled;
    control->alkalineOutputType = config->alkalineOutputType;
    control->alkalineOutputIndex = config->alkalineOutputIndex;
    control->alkalineMotorPower = config->alkalineMotorPower;
    control->alkalineDosingTime_ms = config->alkalineDosingTime_ms;
    control->alkalineDosingInterval_ms = config->alkalineDosingInterval_ms;
    control->alkalineVolumePerDose_mL = config->alkalineVolumePerDose_mL;
    control->alkalineCumulativeVolume_mL = 0.0f;  // Initialize to zero
    control->lastAlkalineDoseTime = 0;
    
    // Create controller instance
    pHController* controller = new pHController(control);
    if (controller == nullptr) {
        Serial.println("[CTRL MGR] Failed to create pH controller instance");
        delete control;
        return false;
    }
    
    // Register in object index
    if (config->index >= MAX_NUM_OBJECTS) {
        Serial.println("[CTRL MGR] pH controller index out of range");
        delete controller;
        delete control;
        return false;
    }
    
    objIndex[config->index].valid = true;
    objIndex[config->index].type = OBJ_T_PH_CONTROL;
    objIndex[config->index].obj = control;
    strncpy(objIndex[config->index].name, config->name, sizeof(objIndex[config->index].name) - 1);
    
    // Create scheduler task (100ms = 10Hz)
    auto taskWrapper = []() {
        if (ControllerManager::phController.controllerInstance) {
            ControllerManager::phController.controllerInstance->update();
        }
    };
    
    ScheduledTask* task = tasks.addTask(taskWrapper, 100);
    if (task == nullptr) {
        Serial.println("[CTRL MGR] Failed to create pH controller task");
        objIndex[config->index].valid = false;
        delete controller;
        delete control;
        return false;
    }
    
    // Store in managed controller
    phController.index = config->index;
    phController.controllerInstance = controller;
    phController.controlObject = control;
    phController.updateTask = task;
    phController.active = true;
    
    Serial.printf("[CTRL MGR] Created pH controller at index %d\n", config->index);
    return true;
}

bool ControllerManager::deletepHController() {
    if (!phController.active) {
        return false;
    }
    
    // Remove scheduler task
    if (phController.updateTask != nullptr) {
        tasks.removeTask(phController.updateTask);
        phController.updateTask = nullptr;
    }
    
    // Delete controller instance
    if (phController.controllerInstance != nullptr) {
        delete phController.controllerInstance;
        phController.controllerInstance = nullptr;
    }
    
    // Delete control object
    if (phController.controlObject != nullptr) {
        delete phController.controlObject;
        phController.controlObject = nullptr;
    }
    
    // Unregister from object index
    if (phController.index < MAX_NUM_OBJECTS) {
        objIndex[phController.index].valid = false;
        objIndex[phController.index].obj = nullptr;
        objIndex[phController.index].name[0] = '\0';
    }
    
    phController.active = false;
    phController.index = 0;
    
    Serial.println("[CTRL MGR] Deleted pH controller");
    return true;
}

bool ControllerManager::configurepHController(const IPC_ConfigpHController_t* config) {
    // For now, recreate the controller
    // Future optimization: update in-place for parameter changes
    return createpHController(config);
}

bool ControllerManager::setpHSetpoint(float setpoint) {
    if (!phController.active || phController.controllerInstance == nullptr) {
        return false;
    }
    
    phController.controllerInstance->setSetpoint(setpoint);
    return true;
}

bool ControllerManager::enablepHController() {
    if (!phController.active || phController.controlObject == nullptr) {
        return false;
    }
    
    phController.controlObject->enabled = true;
    Serial.println("[CTRL MGR] pH controller enabled");
    return true;
}

bool ControllerManager::disablepHController() {
    if (!phController.active || phController.controlObject == nullptr) {
        return false;
    }
    
    phController.controlObject->enabled = false;
    Serial.println("[CTRL MGR] pH controller disabled");
    return true;
}

bool ControllerManager::dosepHAcid() {
    if (!phController.active || phController.controllerInstance == nullptr) {
        return false;
    }
    
    return phController.controllerInstance->doseAcid();
}

bool ControllerManager::dosepHAlkaline() {
    if (!phController.active || phController.controllerInstance == nullptr) {
        return false;
    }
    
    return phController.controllerInstance->doseAlkaline();
}

bool ControllerManager::resetpHAcidVolume() {
    if (!phController.active || phController.controllerInstance == nullptr) {
        return false;
    }
    
    phController.controllerInstance->resetAcidVolume();
    return true;
}

bool ControllerManager::resetpHAlkalineVolume() {
    if (!phController.active || phController.controllerInstance == nullptr) {
        return false;
    }
    
    phController.controllerInstance->resetAlkalineVolume();
    return true;
}

// ============================================================================
// QUERY FUNCTIONS
// ============================================================================

ManagedController* ControllerManager::findController(uint8_t index) {
    int arrIdx = indexToArrayIndex(index);
    if (arrIdx < 0) {
        return nullptr;
    }
    
    ManagedController* ctrl = &controllers[arrIdx];
    return ctrl->active ? ctrl : nullptr;
}

int ControllerManager::getActiveControllerCount() {
    int count = 0;
    for (int i = 0; i < MAX_TEMP_CONTROLLERS; i++) {
        if (controllers[i].active) {
            count++;
        }
    }
    return count;
}

bool ControllerManager::isSlotAvailable(uint8_t index) {
    int arrIdx = indexToArrayIndex(index);
    if (arrIdx < 0) {
        return false;
    }
    
    return !controllers[arrIdx].active;
}

int ControllerManager::getActiveControllers(ManagedController** ctrlArray, int maxCount) {
    int count = 0;
    for (int i = 0; i < MAX_TEMP_CONTROLLERS && count < maxCount; i++) {
        if (controllers[i].active) {
            ctrlArray[count++] = &controllers[i];
        }
    }
    return count;
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

void ControllerManager::registerControllerObject(ManagedController* ctrl) {
    if (ctrl->index >= MAX_NUM_OBJECTS) {
        return;
    }
    
    objIndex[ctrl->index].type = OBJ_T_TEMPERATURE_CONTROL;
    objIndex[ctrl->index].obj = ctrl->controlObject;
    objIndex[ctrl->index].valid = true;
    snprintf(objIndex[ctrl->index].name, sizeof(objIndex[ctrl->index].name),
             "Temperature Controller %d", ctrl->index - 40 + 1);
    
    Serial.printf("[CTRL MGR] Registered controller in objIndex[%d]\n", ctrl->index);
}

void ControllerManager::unregisterControllerObject(ManagedController* ctrl) {
    if (ctrl->index >= MAX_NUM_OBJECTS) {
        return;
    }
    
    objIndex[ctrl->index].valid = false;
    objIndex[ctrl->index].obj = nullptr;
    objIndex[ctrl->index].type = OBJ_T_ANALOG_INPUT;  // Reset to default
    objIndex[ctrl->index].name[0] = '\0';
    
    Serial.printf("[CTRL MGR] Unregistered controller from objIndex[%d]\n", ctrl->index);
}

ScheduledTask* ControllerManager::addControllerTask(ManagedController* ctrl) {
    // Get array index (0-2) from object index (40-42)
    int slot = indexToArrayIndex(ctrl->index);
    if (slot < 0 || slot >= MAX_TEMP_CONTROLLERS) {
        Serial.printf("[CTRL MGR] ERROR: Invalid slot %d for controller %d\n", slot, ctrl->index);
        return nullptr;
    }
    
    // Store controller instance in static array for wrapper function
    controllerInstances[slot] = ctrl->controllerInstance;
    
    // Get the appropriate wrapper function for this slot
    if (taskWrappers[slot] == nullptr) {
        Serial.printf("[CTRL MGR] ERROR: No task wrapper for slot %d\n", slot);
        controllerInstances[slot] = nullptr;
        return nullptr;
    }
    
    // Determine task interval based on output type
    // If using heater output (index 25) in PWM mode, use 1Hz (1000ms) to match 1Hz PWM cycle
    // Otherwise use 10Hz (100ms) for fast control response
    uint32_t taskInterval = 100;  // Default: 100ms (10Hz)
    
    if (ctrl->controlObject != nullptr && ctrl->controlObject->outputIndex == 25) {
        // Check if heater output is in PWM mode
        if (objIndex[25].valid && objIndex[25].type == OBJ_T_DIGITAL_OUTPUT) {
            DigitalOutput_t* heaterOutput = (DigitalOutput_t*)objIndex[25].obj;
            if (heaterOutput != nullptr && heaterOutput->pwmEnabled) {
                taskInterval = 1000;  // 1000ms (1Hz) for heater PWM mode
                Serial.printf("[CTRL MGR] Using 1Hz update rate for heater PWM output\n");
            }
        }
    }
    
    // Add task using the non-capturing wrapper function
    ScheduledTask* task = tasks.addTask(taskWrappers[slot], taskInterval);
    
    if (task != nullptr) {
        Serial.printf("[CTRL MGR] Added scheduler task for controller %d (%dms interval)\n", 
                     ctrl->index, taskInterval);
    } else {
        Serial.printf("[CTRL MGR] ERROR: Failed to add task for controller %d\n", ctrl->index);
        controllerInstances[slot] = nullptr;
    }
    
    return task;
}

void ControllerManager::removeControllerTask(ScheduledTask* task) {
    if (task != nullptr) {
        tasks.removeTask(task);
        Serial.println("[CTRL MGR] Removed scheduler task");
    }
}

void ControllerManager::clearControllerInstance(uint8_t index) {
    // Clear the static array entry when controller is deleted
    int slot = indexToArrayIndex(index);
    if (slot >= 0 && slot < MAX_TEMP_CONTROLLERS) {
        controllerInstances[slot] = nullptr;
    }
}

bool ControllerManager::validateConfig(const IPC_ConfigTempController_t* config) {
    if (config == nullptr) {
        return false;
    }
    
    // Validate index range
    if (config->index < 40 || config->index >= 40 + MAX_TEMP_CONTROLLERS) {
        Serial.printf("[CTRL MGR] Invalid index: %d\n", config->index);
        return false;
    }
    
    // Validate sensor index (must be valid and enrolled in objIndex)
    if (config->pvSourceIndex >= MAX_NUM_OBJECTS) {
        Serial.printf("[CTRL MGR] Invalid sensor index: %d\n", config->pvSourceIndex);
        return false;
    }
    
    if (!objIndex[config->pvSourceIndex].valid) {
        Serial.printf("[CTRL MGR] Sensor index %d not enrolled\n", config->pvSourceIndex);
        return false;
    }
    
    // Validate output index
    if (config->outputIndex >= MAX_NUM_OBJECTS) {
        Serial.printf("[CTRL MGR] Invalid output index: %d\n", config->outputIndex);
        return false;
    }
    
    if (!objIndex[config->outputIndex].valid) {
        Serial.printf("[CTRL MGR] Output index %d not enrolled\n", config->outputIndex);
        return false;
    }
    
    // Validate control method
    if (config->controlMethod > 1) {
        Serial.printf("[CTRL MGR] Invalid control method: %d\n", config->controlMethod);
        return false;
    }
    
    // Validate PID parameters (must be non-negative)
    if (config->kP < 0 || config->kI < 0 || config->kD < 0) {
        Serial.println("[CTRL MGR] Invalid PID parameters (negative)");
        return false;
    }
    
    // Validate output limits
    if (config->outputMin < 0 || config->outputMax > 100 || config->outputMin >= config->outputMax) {
        Serial.printf("[CTRL MGR] Invalid output limits: min=%.1f, max=%.1f\n",
                     config->outputMin, config->outputMax);
        return false;
    }
    
    return true;
}

int ControllerManager::indexToArrayIndex(uint8_t index) {
    if (index < 40 || index >= 40 + MAX_TEMP_CONTROLLERS) {
        return -1;
    }
    return index - 40;
}
