#include "device_manager.h"

// Include sys_init.h to get all type definitions including peripheral devices
// This must come AFTER device_manager.h to break the circular dependency
#include "../sys_init.h"

// Static member initialization
ManagedDevice DeviceManager::devices[MAX_DYNAMIC_DEVICES];
int DeviceManager::deviceCount = 0;
bool DeviceManager::initialized = false;

// ============================================================================
// Static Device Task Wrappers (for scheduler - can't use capturing lambdas)
// ============================================================================

// NOTE: The scheduler's TaskCallback is a plain function pointer that cannot
// capture variables. We need 20 separate non-capturing wrapper functions
// because C++ doesn't allow parameterized function pointers.

// Store device info for each wrapper slot (0-19 maps to device slots)
static void* deviceInstances[MAX_DYNAMIC_DEVICES] = {nullptr};
static IPC_DeviceType deviceTypes[MAX_DYNAMIC_DEVICES] = {IPC_DEV_NONE};

// Generic task wrapper functions - one per slot, dispatches based on device type
#define DEVICE_TASK_WRAPPER(slot) \
    static void deviceTaskWrapper_##slot() { \
        if (deviceInstances[slot] == nullptr) return; \
        switch(deviceTypes[slot]) { \
            case IPC_DEV_HAMILTON_PH: \
                ((HamiltonPHProbe*)deviceInstances[slot])->update(); \
                break; \
            case IPC_DEV_HAMILTON_DO: \
                ((HamiltonArcDO*)deviceInstances[slot])->update(); \
                break; \
            case IPC_DEV_HAMILTON_OD: \
                ((HamiltonArcOD*)deviceInstances[slot])->update(); \
                break; \
            case IPC_DEV_ALICAT_MFC: \
                ((AlicatMFC*)deviceInstances[slot])->update(); \
                break; \
            case IPC_DEV_PRESSURE_CTRL: \
                ((AnaloguePressureController*)deviceInstances[slot])->update(); \
                break; \
            default: \
                break; \
        } \
    }

// Generate 20 wrapper functions (one per dynamic device slot)
DEVICE_TASK_WRAPPER(0)
DEVICE_TASK_WRAPPER(1)
DEVICE_TASK_WRAPPER(2)
DEVICE_TASK_WRAPPER(3)
DEVICE_TASK_WRAPPER(4)
DEVICE_TASK_WRAPPER(5)
DEVICE_TASK_WRAPPER(6)
DEVICE_TASK_WRAPPER(7)
DEVICE_TASK_WRAPPER(8)
DEVICE_TASK_WRAPPER(9)
DEVICE_TASK_WRAPPER(10)
DEVICE_TASK_WRAPPER(11)
DEVICE_TASK_WRAPPER(12)
DEVICE_TASK_WRAPPER(13)
DEVICE_TASK_WRAPPER(14)
DEVICE_TASK_WRAPPER(15)
DEVICE_TASK_WRAPPER(16)
DEVICE_TASK_WRAPPER(17)
DEVICE_TASK_WRAPPER(18)
DEVICE_TASK_WRAPPER(19)

// Array of task wrapper function pointers (indexed by slot 0-19)
typedef void (*TaskWrapperFunc)();
static TaskWrapperFunc taskWrappers[MAX_DYNAMIC_DEVICES] = {
    deviceTaskWrapper_0,  deviceTaskWrapper_1,  deviceTaskWrapper_2,  deviceTaskWrapper_3,
    deviceTaskWrapper_4,  deviceTaskWrapper_5,  deviceTaskWrapper_6,  deviceTaskWrapper_7,
    deviceTaskWrapper_8,  deviceTaskWrapper_9,  deviceTaskWrapper_10, deviceTaskWrapper_11,
    deviceTaskWrapper_12, deviceTaskWrapper_13, deviceTaskWrapper_14, deviceTaskWrapper_15,
    deviceTaskWrapper_16, deviceTaskWrapper_17, deviceTaskWrapper_18, deviceTaskWrapper_19
};

// ============================================================================
// Initialization
// ============================================================================

bool DeviceManager::init() {
    if (initialized) {
        return true;
    }
    
    // Initialize all device slots
    for (int i = 0; i < MAX_DYNAMIC_DEVICES; i++) {
        devices[i].type = IPC_DEV_NONE;
        devices[i].controlIndex = 0;
        devices[i].startSensorIndex = 0;
        devices[i].sensorCount = 0;
        devices[i].deviceInstance = nullptr;
        devices[i].controlObject = nullptr;
        devices[i].updateTask = nullptr;
        devices[i].active = false;
        devices[i].fault = false;
        devices[i].message[0] = '\0';
        
        // Initialize static wrapper arrays
        deviceInstances[i] = nullptr;
        deviceTypes[i] = IPC_DEV_NONE;
    }
    
    deviceCount = 0;
    initialized = true;
    
    Serial.println("[DEV MGR] Device Manager initialized");
    return true;
}

// ============================================================================
// Device Lifecycle - Create
// ============================================================================

bool DeviceManager::createDevice(uint8_t startIndex, const IPC_DeviceConfig_t* config) {
    if (!initialized) {
        Serial.println("[DEV MGR] ERROR: Not initialized");
        return false;
    }
    
    // Validate index range - allow control (50-69) and sensor (70-99) ranges
    if ((startIndex < 50 || startIndex > 69) && 
        (startIndex < 70 || startIndex > 99)) {
        Serial.printf("[DEV MGR] ERROR: Invalid start index %d (must be 50-69 or 70-99)\n", startIndex);
        return false;
    }
    
    // Calculate control index
    // Control-only devices (50-69): control index = start index
    // Sensor devices (70-99): control index = start index - 20
    uint8_t controlIndex;
    if (startIndex >= 50 && startIndex <= 69) {
        // Control-only device
        controlIndex = startIndex;
    } else {
        // Sensor device: sensor 70-99 → control 50-79
        controlIndex = startIndex - 20;
    }
    
    // Validate control index range (50-69)
    if (controlIndex < 50 || controlIndex >= 70) {
        Serial.printf("[DEV MGR] ERROR: Invalid control index %d (from start index %d)\n", 
                     controlIndex, startIndex);
        return false;
    }
    
    // Check if control index is already in use
    if (objIndex[controlIndex].valid) {
        Serial.printf("[DEV MGR] ERROR: Control index %d already in use\n", controlIndex);
        return false;
    }
    
    // Validate configuration
    if (!validateConfig(config)) {
        Serial.println("[DEV MGR] ERROR: Invalid device configuration");
        return false;
    }
    
    // Check if device already exists at this index
    ManagedDevice* existing = findDevice(startIndex);
    if (existing != nullptr) {
        Serial.printf("[DEV MGR] ERROR: Device already exists at index %d\n", startIndex);
        return false;
    }
    
    // Get expected object count for this device type
    uint8_t objectCount = getObjectCount((IPC_DeviceType)config->deviceType);
    if (objectCount == 0xFF) {  // 0xFF indicates unknown type
        Serial.printf("[DEV MGR] ERROR: Unknown device type %d\n", config->deviceType);
        return false;
    }
    
    // Check if sensor slots are available (only if device has sensors)
    // Control-only devices (objectCount=0) don't need sensor slots
    if (objectCount > 0) {
        if (!isSlotAvailable(startIndex, objectCount)) {
            Serial.printf("[DEV MGR] ERROR: Slots %d-%d not available\n", 
                         startIndex, startIndex + objectCount - 1);
            return false;
        }
    } else {
        Serial.printf("[DEV MGR] Control-only device (no sensor slots needed)\n");
    }
    
    // Find a free device slot
    int deviceSlot = -1;
    for (int i = 0; i < MAX_DYNAMIC_DEVICES; i++) {
        if (devices[i].type == IPC_DEV_NONE) {
            deviceSlot = i;
            break;
        }
    }
    
    if (deviceSlot == -1) {
        Serial.println("[DEV MGR] ERROR: No free device slots");
        return false;
    }
    
    // Create the device instance
    void* instance = createDeviceInstance(config);
    if (instance == nullptr) {
        Serial.printf("[DEV MGR] ERROR: Failed to create device instance (type %d)\n", 
                     config->deviceType);
        return false;
    }
    
    // Populate managed device structure first
    ManagedDevice* dev = &devices[deviceSlot];
    dev->deviceInstance = instance;
    dev->type = (IPC_DeviceType)config->deviceType;
    
    // Get control object pointer from device instance
    void* controlObject = getDeviceControlObject(instance, (IPC_DeviceType)config->deviceType);
    if (controlObject == nullptr) {
        Serial.printf("[DEV MGR] ERROR: Failed to get control object for device type %d\n", 
                     config->deviceType);
        destroyDeviceInstance(dev);  // Clean up
        dev->deviceInstance = nullptr;
        dev->type = IPC_DEV_NONE;
        return false;
    }
    
    // Complete managed device structure
    memcpy(&dev->config, config, sizeof(IPC_DeviceConfig_t));
    dev->controlIndex = controlIndex;
    dev->startSensorIndex = startIndex;
    dev->sensorCount = objectCount;
    dev->controlObject = controlObject;
    dev->active = false;  // Will be set true after successful registration
    dev->fault = false;
    strcpy(dev->message, "Created");
    
    // Register control object in object index (indices 50-69)
    objIndex[controlIndex].type = OBJ_T_DEVICE_CONTROL;
    objIndex[controlIndex].obj = controlObject;
    objIndex[controlIndex].valid = true;
    snprintf(objIndex[controlIndex].name, sizeof(objIndex[controlIndex].name),
             "Device Control %d", controlIndex);
    
    // Initialize control object fields
    DeviceControl_t* ctrl = (DeviceControl_t*)controlObject;
    ctrl->startSensorIndex = startIndex;
    ctrl->sensorCount = objectCount;
    ctrl->slaveID = config->address;
    ctrl->deviceType = config->deviceType;
    ctrl->connected = false;
    ctrl->fault = false;
    ctrl->newMessage = false;
    strcpy(ctrl->message, "Initializing");
    
    // Register sensor objects in object index (indices 70-99)
    registerDeviceObjects(dev);
    
    // Add scheduler task for periodic updates
    dev->updateTask = addDeviceTask(dev);
    if (dev->updateTask == nullptr) {
        Serial.println("[DEV MGR] WARNING: Failed to add update task");
        // Continue anyway - device can still be queried manually
    }
    
    // Mark as active
    dev->active = true;
    dev->config.objectCount = objectCount;  // Update config with actual count
    deviceCount++;
    
    Serial.printf("[DEV MGR] ✓ Device created: type=%d, control=%d, sensors=%d-%d\n",
                 config->deviceType, controlIndex, startIndex, startIndex + objectCount - 1);
    
    return true;
}

// ============================================================================
// Device Lifecycle - Delete
// ============================================================================

bool DeviceManager::deleteDevice(uint8_t startIndex) {
    ManagedDevice* dev = findDevice(startIndex);
    if (dev == nullptr) {
        Serial.printf("[DEV MGR] ERROR: Device not found at index %d\n", startIndex);
        return false;
    }
    
    // Remove scheduler task
    if (dev->updateTask != nullptr) {
        removeDeviceTask(dev->updateTask);
        dev->updateTask = nullptr;
    }
    
    // Unregister control object from object index (50-69)
    if (dev->controlIndex > 0 && dev->controlIndex < MAX_NUM_OBJECTS) {
        objIndex[dev->controlIndex].valid = false;
        objIndex[dev->controlIndex].obj = nullptr;
        objIndex[dev->controlIndex].type = OBJ_T_ANALOG_INPUT;  // Reset to default
        objIndex[dev->controlIndex].name[0] = '\0';
    }
    
    // Unregister sensor objects from index (70-99)
    unregisterDeviceObjects(dev);
    
    // Destroy device instance
    destroyDeviceInstance(dev);
    
    // Clear device slot
    dev->type = IPC_DEV_NONE;
    dev->controlIndex = 0;
    dev->startSensorIndex = 0;
    dev->sensorCount = 0;
    dev->deviceInstance = nullptr;
    dev->controlObject = nullptr;
    dev->active = false;
    dev->fault = false;
    dev->message[0] = '\0';
    
    deviceCount--;
    
    Serial.printf("[DEV MGR] ✓ Device deleted: control=%d, sensors=%d-%d\n", 
                 dev->controlIndex, startIndex, startIndex + dev->sensorCount - 1);
    return true;
}

// ============================================================================
// Device Lifecycle - Configure
// ============================================================================

bool DeviceManager::configureDevice(uint8_t startIndex, const IPC_DeviceConfig_t* config) {
    // For now, require delete and recreate
    // Future: Support in-place configuration changes
    Serial.printf("[DEV MGR] Config update: delete and recreate required for index %d\n", startIndex);
    
    if (!deleteDevice(startIndex)) {
        return false;
    }
    
    return createDevice(startIndex, config);
}

// ============================================================================
// Device Query
// ============================================================================

ManagedDevice* DeviceManager::findDevice(uint8_t startIndex) {
    for (int i = 0; i < MAX_DYNAMIC_DEVICES; i++) {
        if (devices[i].type != IPC_DEV_NONE && devices[i].startSensorIndex == startIndex) {
            return &devices[i];
        }
    }
    return nullptr;
}

ManagedDevice* DeviceManager::findDeviceByControlIndex(uint8_t controlIndex) {
    for (int i = 0; i < MAX_DYNAMIC_DEVICES; i++) {
        if (devices[i].type != IPC_DEV_NONE && devices[i].controlIndex == controlIndex) {
            return &devices[i];
        }
    }
    return nullptr;
}

int DeviceManager::getActiveDeviceCount() {
    return deviceCount;
}

bool DeviceManager::isSlotAvailable(uint8_t startIndex, uint8_t objectCount) {
    // Check range
    if (startIndex < 60 || startIndex + objectCount > 80) {
        return false;
    }
    
    // Check if any of the required slots are already in use
    for (uint8_t i = 0; i < objectCount; i++) {
        uint8_t idx = startIndex + i;
        if (objIndex[idx].valid) {
            return false;  // Slot already occupied
        }
    }
    
    return true;
}

int DeviceManager::getActiveDevices(ManagedDevice** deviceArray, int maxCount) {
    int count = 0;
    for (int i = 0; i < MAX_DYNAMIC_DEVICES && count < maxCount; i++) {
        if (devices[i].type != IPC_DEV_NONE) {
            deviceArray[count++] = &devices[i];
        }
    }
    return count;
}

// ============================================================================
// Internal Helpers - Factory Pattern
// ============================================================================

void* DeviceManager::createDeviceInstance(const IPC_DeviceConfig_t* config) {
    // Modbus devices
    if (config->busType == IPC_BUS_MODBUS_RTU) {
        // Validate bus index (0-3)
        if (config->busIndex > 3) {
            Serial.printf("[DEV MGR] ERROR: Invalid Modbus port %d\n", config->busIndex);
            return nullptr;
        }
        
        ModbusDriver_t* driver = &modbusDriver[config->busIndex];
        
        switch((IPC_DeviceType)config->deviceType) {
            case IPC_DEV_HAMILTON_PH:
                Serial.printf("[DEV MGR] Creating Hamilton pH probe (port %d, ID %d)\n",
                             config->busIndex, config->address);
                return new HamiltonPHProbe(driver, config->address);
            
            case IPC_DEV_HAMILTON_DO:
                Serial.printf("[DEV MGR] Creating Hamilton DO probe (port %d, ID %d)\n",
                             config->busIndex, config->address);
                return new HamiltonArcDO(driver, config->address);
            
            case IPC_DEV_HAMILTON_OD:
                Serial.printf("[DEV MGR] Creating Hamilton OD probe (port %d, ID %d)\n",
                             config->busIndex, config->address);
                return new HamiltonArcOD(driver, config->address);
            
            case IPC_DEV_ALICAT_MFC: {
                Serial.printf("[DEV MGR] Creating Alicat MFC (port %d, ID %d, max %.1f mL/min)\n",
                             config->busIndex, config->address, config->maxFlowRate_mL_min);
                AlicatMFC* mfc = new AlicatMFC(driver, config->address);
                if (mfc && config->maxFlowRate_mL_min > 0) {
                    mfc->setMaxFlowRate(config->maxFlowRate_mL_min);
                }
                return mfc;
            }
            
            default:
                Serial.printf("[DEV MGR] ERROR: Unsupported Modbus device type %d\n",
                             config->deviceType);
                return nullptr;
        }
    }
    
    // I2C devices (future)
    // SPI devices (future)
    
    // Analog output devices
    if (config->busType == IPC_BUS_ANALOG) {
        switch(config->deviceType) {
            case IPC_DEV_PRESSURE_CTRL: {
                Serial.printf("[DEV MGR] Creating Pressure Controller (DAC %d)\n",
                             config->busIndex);
                
                // Calibration will be applied via config message after creation
                return new AnaloguePressureController(config->busIndex);
            }
            
            default:
                Serial.printf("[DEV MGR] ERROR: Unsupported analog device type %d\n",
                             config->deviceType);
                return nullptr;
        }
    }
    
    Serial.printf("[DEV MGR] ERROR: Unsupported bus type %d\n", config->busType);
    return nullptr;
}

void* DeviceManager::getDeviceControlObject(void* instance, IPC_DeviceType type) {
    if (instance == nullptr) {
        return nullptr;
    }
    
    // Cast device instance and call getControlObject() based on type
    switch(type) {
        case IPC_DEV_HAMILTON_PH:
            return ((HamiltonPHProbe*)instance)->getControlObject();
        
        case IPC_DEV_HAMILTON_DO:
            return ((HamiltonArcDO*)instance)->getControlObject();
        
        case IPC_DEV_HAMILTON_OD:
            return ((HamiltonArcOD*)instance)->getControlObject();
        
        case IPC_DEV_ALICAT_MFC:
            return ((AlicatMFC*)instance)->getControlObject();
        
        case IPC_DEV_PRESSURE_CTRL:
            return ((AnaloguePressureController*)instance)->getControlObject();
        
        default:
            Serial.printf("[DEV MGR] ERROR: Unknown device type %d for control object\n", type);
            return nullptr;
    }
}

void DeviceManager::destroyDeviceInstance(ManagedDevice* dev) {
    if (dev->deviceInstance == nullptr) {
        return;
    }
    
    // Cast and delete based on type
    switch(dev->type) {
        case IPC_DEV_HAMILTON_PH:
            delete (HamiltonPHProbe*)dev->deviceInstance;
            break;
        
        case IPC_DEV_HAMILTON_DO:
            delete (HamiltonArcDO*)dev->deviceInstance;
            break;
        
        case IPC_DEV_HAMILTON_OD:
            delete (HamiltonArcOD*)dev->deviceInstance;
            break;
        
        case IPC_DEV_ALICAT_MFC:
            delete (AlicatMFC*)dev->deviceInstance;
            break;
        
        case IPC_DEV_PRESSURE_CTRL:
            delete (AnaloguePressureController*)dev->deviceInstance;
            break;
        
        default:
            Serial.printf("[DEV MGR] WARNING: Unknown device type %d in destroy\n", dev->type);
            break;
    }
    
    dev->deviceInstance = nullptr;
}

// ============================================================================
// Internal Helpers - Object Index Registration
// ============================================================================

void DeviceManager::registerDeviceObjects(ManagedDevice* dev) {
    switch(dev->type) {
        case IPC_DEV_HAMILTON_PH: {
            HamiltonPHProbe* probe = (HamiltonPHProbe*)dev->deviceInstance;
            
            // Register pH sensor at startSensorIndex
            objIndex[dev->startSensorIndex].type = OBJ_T_PH_SENSOR;
            objIndex[dev->startSensorIndex].obj = &probe->getPhSensor();
            sprintf(objIndex[dev->startSensorIndex].name, "pH Sensor %d", dev->startSensorIndex);
            objIndex[dev->startSensorIndex].valid = true;
            
            // Register temperature sensor at startSensorIndex + 1
            objIndex[dev->startSensorIndex + 1].type = OBJ_T_TEMPERATURE_SENSOR;
            objIndex[dev->startSensorIndex + 1].obj = &probe->getTemperatureSensor();
            sprintf(objIndex[dev->startSensorIndex + 1].name, "pH Temp %d", dev->startSensorIndex);
            objIndex[dev->startSensorIndex + 1].valid = true;
            
            Serial.printf("[DEV MGR] Registered pH probe objects at %d-%d\n",
                         dev->startSensorIndex, dev->startSensorIndex + 1);
            break;
        }
        
        case IPC_DEV_HAMILTON_DO: {
            HamiltonArcDO* probe = (HamiltonArcDO*)dev->deviceInstance;
            
            // Register DO sensor
            objIndex[dev->startSensorIndex].type = OBJ_T_DISSOLVED_OXYGEN_SENSOR;
            objIndex[dev->startSensorIndex].obj = &probe->getDOSensor();
            sprintf(objIndex[dev->startSensorIndex].name, "DO Sensor %d", dev->startSensorIndex);
            objIndex[dev->startSensorIndex].valid = true;
            
            // Register temperature sensor
            objIndex[dev->startSensorIndex + 1].type = OBJ_T_TEMPERATURE_SENSOR;
            objIndex[dev->startSensorIndex + 1].obj = &probe->getTemperatureSensor();
            sprintf(objIndex[dev->startSensorIndex + 1].name, "DO Temp %d", dev->startSensorIndex);
            objIndex[dev->startSensorIndex + 1].valid = true;
            
            Serial.printf("[DEV MGR] Registered DO probe objects at %d-%d\n",
                         dev->startSensorIndex, dev->startSensorIndex + 1);
            break;
        }
        
        case IPC_DEV_HAMILTON_OD: {
            HamiltonArcOD* probe = (HamiltonArcOD*)dev->deviceInstance;
            
            // Register OD sensor
            objIndex[dev->startSensorIndex].type = OBJ_T_OPTICAL_DENSITY_SENSOR;
            objIndex[dev->startSensorIndex].obj = &probe->getODSensor();
            sprintf(objIndex[dev->startSensorIndex].name, "OD Sensor %d", dev->startSensorIndex);
            objIndex[dev->startSensorIndex].valid = true;
            
            // Register temperature sensor
            objIndex[dev->startSensorIndex + 1].type = OBJ_T_TEMPERATURE_SENSOR;
            objIndex[dev->startSensorIndex + 1].obj = &probe->getTemperatureSensor();
            sprintf(objIndex[dev->startSensorIndex + 1].name, "OD Temp %d", dev->startSensorIndex);
            objIndex[dev->startSensorIndex + 1].valid = true;
            
            Serial.printf("[DEV MGR] Registered OD probe objects at %d-%d\n",
                         dev->startSensorIndex, dev->startSensorIndex + 1);
            break;
        }
        
        case IPC_DEV_ALICAT_MFC: {
            AlicatMFC* mfc = (AlicatMFC*)dev->deviceInstance;
            
            // Register flow sensor
            objIndex[dev->startSensorIndex].type = OBJ_T_FLOW_SENSOR;
            objIndex[dev->startSensorIndex].obj = &mfc->getFlowSensor();
            sprintf(objIndex[dev->startSensorIndex].name, "MFC Flow %d", dev->startSensorIndex);
            objIndex[dev->startSensorIndex].valid = true;
            
            // Register pressure sensor
            objIndex[dev->startSensorIndex + 1].type = OBJ_T_PRESSURE_SENSOR;
            objIndex[dev->startSensorIndex + 1].obj = &mfc->getPressureSensor();
            sprintf(objIndex[dev->startSensorIndex + 1].name, "MFC Pressure %d", dev->startSensorIndex);
            objIndex[dev->startSensorIndex + 1].valid = true;
            
            Serial.printf("[DEV MGR] Registered MFC objects at %d-%d\n",
                         dev->startSensorIndex, dev->startSensorIndex + 1);
            break;
        }
        
        case IPC_DEV_PRESSURE_CTRL: {
            // Pressure controller has 1 sensor object (actual DAC feedback)
            AnaloguePressureController* controller = (AnaloguePressureController*)dev->deviceInstance;
            
            // Register actual pressure sensor (reads back DAC output)
            PressureSensor_t* actualSensor = (PressureSensor_t*)controller->getSensorObject(0);
            if (actualSensor) {
                objIndex[dev->startSensorIndex].type = OBJ_T_PRESSURE_SENSOR;
                objIndex[dev->startSensorIndex].obj = actualSensor;
                strncpy(objIndex[dev->startSensorIndex].name, "Pressure Actual", 
                       sizeof(objIndex[dev->startSensorIndex].name) - 1);
                objIndex[dev->startSensorIndex].valid = true;
            }
            
            Serial.printf("[DEV MGR] Registered Pressure Controller sensor at %d\n",
                         dev->startSensorIndex);
            break;
        }
        
        default:
            Serial.printf("[DEV MGR] WARNING: Unknown device type %d in register\n", dev->type);
            break;
    }
}

void DeviceManager::unregisterDeviceObjects(ManagedDevice* dev) {
    for (uint8_t i = 0; i < dev->sensorCount; i++) {
        uint8_t idx = dev->startSensorIndex + i;
        objIndex[idx].type = OBJ_T_ANALOG_INPUT;  // Reset to default
        objIndex[idx].obj = nullptr;
        objIndex[idx].name[0] = '\0';
        objIndex[idx].valid = false;
    }
    
    Serial.printf("[DEV MGR] Unregistered %d sensor objects starting at %d\n",
                 dev->sensorCount, dev->startSensorIndex);
}

// ============================================================================
// Internal Helpers - Task Management
// ============================================================================

ScheduledTask* DeviceManager::addDeviceTask(ManagedDevice* dev) {
    // Find a free slot for this device instance
    int slot = -1;
    for (int i = 0; i < MAX_DYNAMIC_DEVICES; i++) {
        if (deviceInstances[i] == nullptr) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        Serial.println("[DEV MGR] ERROR: No free task wrapper slots");
        return nullptr;
    }
    
    // Store device instance and type in static arrays
    deviceInstances[slot] = dev->deviceInstance;
    deviceTypes[slot] = dev->type;
    
    // Get the appropriate wrapper function for this slot
    if (taskWrappers[slot] == nullptr) {
        Serial.printf("[DEV MGR] ERROR: No task wrapper for slot %d\n", slot);
        deviceInstances[slot] = nullptr;
        deviceTypes[slot] = IPC_DEV_NONE;
        return nullptr;
    }
    
    // Add task using the non-capturing wrapper function
    ScheduledTask* task = tasks.addTask(taskWrappers[slot], 2000, true, false);
    
    if (task == nullptr) {
        Serial.printf("[DEV MGR] ERROR: Failed to add task for slot %d\n", slot);
        deviceInstances[slot] = nullptr;
        deviceTypes[slot] = IPC_DEV_NONE;
        return nullptr;
    }
    
    Serial.printf("[DEV MGR] Added task for device type %d in slot %d\n", dev->type, slot);
    return task;
}

void DeviceManager::removeDeviceTask(ScheduledTask* task) {
    if (task == nullptr) {
        return;
    }
    
    // Find which slot this task belongs to and clear it
    for (int i = 0; i < MAX_DYNAMIC_DEVICES; i++) {
        if (devices[i].updateTask == task) {
            deviceInstances[i] = nullptr;
            deviceTypes[i] = IPC_DEV_NONE;
            Serial.printf("[DEV MGR] Cleared task wrapper slot %d\n", i);
            break;
        }
    }
    
    tasks.removeTask(task);
}

// ============================================================================
// Internal Helpers - Utilities
// ============================================================================

uint8_t DeviceManager::getObjectCount(IPC_DeviceType type) {
    switch(type) {
        case IPC_DEV_HAMILTON_PH:
        case IPC_DEV_HAMILTON_DO:
        case IPC_DEV_HAMILTON_OD:
        case IPC_DEV_ALICAT_MFC:
            return 2;  // Device + temperature (or flow + pressure)
        
        case IPC_DEV_BME280:
        case IPC_DEV_SCD40:
        case IPC_DEV_INA260:
            return 3;  // Three sensors
        
        case IPC_DEV_PRESSURE_CTRL:
            return 1;  // 1 sensor (actual DAC feedback)
        
        default:
            return 0xFF;  // Unknown type
    }
}

bool DeviceManager::validateConfig(const IPC_DeviceConfig_t* config) {
    // Check device type is not NONE
    if (config->deviceType == IPC_DEV_NONE) {
        Serial.println("[DEV MGR] ERROR: Device type is NONE");
        return false;
    }
    
    // Validate bus-specific parameters
    switch(config->busType) {
        case IPC_BUS_MODBUS_RTU:
            // Modbus port 0-3
            if (config->busIndex > 3) {
                Serial.printf("[DEV MGR] ERROR: Invalid Modbus port %d\n", config->busIndex);
                return false;
            }
            // Slave ID 1-247
            if (config->address < 1 || config->address > 247) {
                Serial.printf("[DEV MGR] ERROR: Invalid Modbus slave ID %d\n", config->address);
                return false;
            }
            break;
        
        case IPC_BUS_I2C:
            // I2C address 0x00-0x7F (7-bit)
            if (config->address > 0x7F) {
                Serial.printf("[DEV MGR] ERROR: Invalid I2C address 0x%02X\n", config->address);
                return false;
            }
            break;
        
        case IPC_BUS_ANALOG:
            // Analog bus: DAC index 0-1
            if (config->busIndex > 1) {
                Serial.printf("[DEV MGR] ERROR: Invalid DAC index %d (must be 0-1)\n", config->busIndex);
                return false;
            }
            // No address validation needed for analog devices
            break;
        
        default:
            Serial.printf("[DEV MGR] ERROR: Unsupported bus type %d\n", config->busType);
            return false;
    }
    
    return true;
}
