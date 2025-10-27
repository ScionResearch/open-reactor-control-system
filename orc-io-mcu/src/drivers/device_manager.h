#pragma once

#include <Arduino.h>
#include "ipc/ipc_protocol.h"

// Forward declarations
class ScheduledTask;
class TaskScheduler;
class ModbusDriver_t;

// Forward declarations for device classes
class HamiltonPHProbe;
class AlicatMFC;
class HamiltonArcDO;
class HamiltonArcOD;

// Maximum number of dynamic devices (30 device slots, each can have control + sensors)
#define MAX_DYNAMIC_DEVICES 30

/**
 * @brief Managed Device Entry
 * 
 * Tracks all information needed for a dynamically created peripheral device
 */
struct ManagedDevice {
    IPC_DeviceType type;           // Device type (from IPC_DeviceType enum)
    IPC_DeviceConfig_t config;     // Device configuration
    uint8_t controlIndex;          // Control object index (50-69), 0 if none
    uint8_t startSensorIndex;      // First sensor index allocated (70-99)
    uint8_t sensorCount;           // Number of sensor objects (1-4)
    void* deviceInstance;          // Pointer to device class instance
    void* controlObject;           // Pointer to DeviceControl_t object
    ScheduledTask* updateTask;     // Scheduler task for periodic update()
    bool active;                // Device is active and updating
    bool fault;                 // Device has a fault condition
    char message[100];          // Status/error message
};

/**
 * @brief Device Manager - Dynamic peripheral device lifecycle management
 * 
 * Manages the creation, configuration, and deletion of dynamic peripheral
 * devices (Modbus sensors, I2C sensors, etc.) using dual-index model:
 * - Control objects: indices 50-69 (20 slots)
 * - Sensor objects: indices 70-99 (30 slots)
 * 
 * Features:
 * - Factory pattern for device instantiation
 * - Automatic object index registration
 * - Scheduler task management for periodic updates
 * - Fault tracking and status reporting
 */
class DeviceManager {
public:
    /**
     * @brief Initialize the Device Manager
     * @return true if initialization successful
     */
    static bool init();
    
    // ========================================================================
    // Device Lifecycle
    // ========================================================================
    
    /**
     * @brief Create a new dynamic device instance
     * 
     * @param startSensorIndex First sensor index to allocate (70-99)
     * @param config Device configuration (type, bus, address)
     * @return true if device created successfully
     */
    static bool createDevice(uint8_t startIndex, const IPC_DeviceConfig_t* config);
    
    /**
     * @brief Delete a dynamic device instance
     * 
     * Stops the update task, frees object indices, and destroys the device instance.
     * 
     * @param startIndex First object index of device to delete
     * @return true if device deleted successfully
     */
    static bool deleteDevice(uint8_t startIndex);
    
    /**
     * @brief Update device configuration
     * 
     * Currently requires delete and recreate. Future: in-place config update.
     * 
     * @param startIndex First object index of device
     * @param config New device configuration
     * @return true if configuration updated successfully
     */
    static bool configureDevice(uint8_t startIndex, const IPC_DeviceConfig_t* config);
    
    // ========================================================================
    // Device Query
    // ========================================================================
    
    /**
     * @brief Find a managed device by its starting index
     * 
     * @param startIndex First object index of device
     * @return Pointer to ManagedDevice or nullptr if not found
     */
    static ManagedDevice* findDevice(uint8_t startIndex);
    
    /**
     * @brief Find a device by its control object index
     * 
     * @param controlIndex Control object index (50-69)
     * @return Pointer to ManagedDevice or nullptr if not found
     */
    static ManagedDevice* findDeviceByControlIndex(uint8_t controlIndex);
    
    /**
     * @brief Get the number of active devices
     * @return Number of active devices
     */
    static int getActiveDeviceCount();
    
    /**
     * @brief Check if object index slots are available
     * 
     * @param startIndex First index to check
     * @param objectCount Number of consecutive indices needed
     * @return true if slots are available and not in use
     */
    static bool isSlotAvailable(uint8_t startIndex, uint8_t objectCount);
    
    /**
     * @brief Get all active devices
     * 
     * @param devices Output array to fill with device pointers
     * @param maxCount Maximum number of devices to return
     * @return Number of devices returned
     */
    static int getActiveDevices(ManagedDevice** devices, int maxCount);
    
private:
    static ManagedDevice devices[MAX_DYNAMIC_DEVICES];
    static int deviceCount;
    static bool initialized;
    
    // ========================================================================
    // Internal Helpers
    // ========================================================================
    
    /**
     * @brief Create a device instance based on type (factory pattern)
     * 
     * @param config Device configuration
     * @return Pointer to device instance or nullptr on failure
     */
    static void* createDeviceInstance(const IPC_DeviceConfig_t* config);
    
    /**
     * @brief Get control object pointer from device instance
     * 
     * @param instance Pointer to device instance
     * @param type Device type enum
     * @return Pointer to DeviceControl_t object or nullptr on failure
     */
    static void* getDeviceControlObject(void* instance, IPC_DeviceType type);
    
    /**
     * @brief Destroy a device instance and free memory
     * 
     * @param dev Managed device to destroy
     */
    static void destroyDeviceInstance(ManagedDevice* dev);
    
    /**
     * @brief Register device's sensor objects in the object index
     * 
     * @param dev Managed device with sensor objects to register
     */
    static void registerDeviceObjects(ManagedDevice* dev);
    
    /**
     * @brief Unregister device's sensor objects from the object index
     * 
     * @param dev Managed device with sensor objects to unregister
     */
    static void unregisterDeviceObjects(ManagedDevice* dev);
    
    /**
     * @brief Add a scheduler task for periodic device updates
     * 
     * @param dev Managed device to add task for
     * @return Task pointer or nullptr on failure
     */
    static ScheduledTask* addDeviceTask(ManagedDevice* dev);
    
    /**
     * @brief Remove a scheduler task
     * 
     * @param task Task pointer to remove
     */
    static void removeDeviceTask(ScheduledTask* task);
    
    /**
     * @brief Get the expected object count for a device type
     * 
     * @param type Device type
     * @return Number of object indices needed (1-4)
     */
    static uint8_t getObjectCount(IPC_DeviceType type);
    
    /**
     * @brief Validate device configuration parameters
     * 
     * @param config Device configuration to validate
     * @return true if configuration is valid
     */
    static bool validateConfig(const IPC_DeviceConfig_t* config);
};

// External reference to task scheduler
extern TaskScheduler tasks;
