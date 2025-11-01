#pragma once

#include <Arduino.h>
#include "sys_init.h"

// Forward declarations
class ScheduledTask;
class TaskScheduler;

// Maximum number of temperature controllers (matches ioConfig.h)
#define MAX_TEMP_CONTROLLERS 3

/**
 * @brief Managed Controller Entry
 * 
 * Tracks all information needed for a dynamically created temperature controller
 */
struct ManagedController {
    uint8_t index;                          // Controller object index (40-42)
    TemperatureController* controllerInstance;  // Pointer to controller class instance
    TemperatureControl_t* controlObject;    // Pointer to control structure in objIndex
    ScheduledTask* updateTask;              // Scheduler task for periodic update()
    bool active;                            // Controller is active and updating
    char message[100];                      // Status/error message
};

/**
 * @brief Controller Manager - Temperature controller lifecycle management
 * 
 * Manages the creation, configuration, and deletion of temperature controllers
 * using the object index system (indices 40-42).
 * 
 * Features:
 * - Factory pattern for controller instantiation
 * - Automatic object index registration
 * - Scheduler task management for periodic updates (100ms = 10Hz)
 * - Fault tracking and status reporting
 * 
 * Similar pattern to DeviceManager but for local controllers
 */
class ControllerManager {
public:
    /**
     * @brief Initialize the Controller Manager
     * @return true if initialization successful
     */
    static bool init();
    
    // ========================================================================
    // Controller Lifecycle
    // ========================================================================
    
    /**
     * @brief Create a new temperature controller instance
     * 
     * @param index Controller index (40-42)
     * @param config Controller configuration from IPC
     * @return true if controller created successfully
     */
    static bool createController(uint8_t index, const IPC_ConfigTempController_t* config);
    
    /**
     * @brief Delete a temperature controller instance
     * 
     * Stops the update task, frees object index, and destroys the controller instance.
     * 
     * @param index Controller index (40-42)
     * @return true if controller deleted successfully
     */
    static bool deleteController(uint8_t index);
    
    /**
     * @brief Update controller configuration
     * 
     * Updates existing controller with new parameters without recreating it.
     * 
     * @param index Controller index (40-42)
     * @param config New controller configuration
     * @return true if configuration updated successfully
     */
    static bool configureController(uint8_t index, const IPC_ConfigTempController_t* config);
    
    // ========================================================================
    // Controller Control Commands
    // ========================================================================
    
    /**
     * @brief Set controller setpoint
     * 
     * @param index Controller index (40-42)
     * @param setpoint New target setpoint
     * @return true if setpoint updated successfully
     */
    static bool setSetpoint(uint8_t index, float setpoint);
    
    /**
     * @brief Enable controller
     * 
     * @param index Controller index (40-42)
     * @return true if controller enabled successfully
     */
    static bool enableController(uint8_t index);
    
    /**
     * @brief Disable controller
     * 
     * @param index Controller index (40-42)
     * @return true if controller disabled successfully
     */
    static bool disableController(uint8_t index);
    
    /**
     * @brief Start autotune sequence for temperature controller
     * 
     * @param index Controller index (40-42)
     * @param targetSetpoint Temperature to tune around
     * @param outputStep Output step size for relay (default 100%)
     * @return true if autotune started successfully
     */
    static bool startAutotune(uint8_t index, float targetSetpoint, float outputStep = 100.0f);
    
    /**
     * @brief Stop autotune sequence
     * 
     * @param index Controller index (40-42)
     * @return true if autotune stopped successfully
     */
    static bool stopAutotune(uint8_t index);
    
    // ========================================================================
    // Controller Query
    // ========================================================================
    
    /**
     * @brief Find a managed controller by its index
     * 
     * @param index Controller index (40-42)
     * @return Pointer to ManagedController or nullptr if not found
     */
    static ManagedController* findController(uint8_t index);
    
    /**
     * @brief Get the number of active controllers
     * @return Number of active controllers
     */
    static int getActiveControllerCount();
    
    /**
     * @brief Check if controller index is available
     * 
     * @param index Controller index to check (40-42)
     * @return true if index is available (not in use)
     */
    static bool isSlotAvailable(uint8_t index);
    
    /**
     * @brief Get all active controllers
     * 
     * @param controllers Output array to fill with controller pointers
     * @param maxCount Maximum number of controllers to return
     * @return Number of controllers returned
     */
    static int getActiveControllers(ManagedController** controllers, int maxCount);
    
private:
    static ManagedController controllers[MAX_TEMP_CONTROLLERS];  // Controller array (3 slots)
    static bool initialized;
    
    // ========================================================================
    // Internal Helpers
    // ========================================================================
    
    /**
     * @brief Register controller object in the object index
     * 
     * @param ctrl Managed controller to register
     */
    static void registerControllerObject(ManagedController* ctrl);
    
    /**
     * @brief Unregister controller object from the object index
     * 
     * @param ctrl Managed controller to unregister
     */
    static void unregisterControllerObject(ManagedController* ctrl);
    
    /**
     * @brief Add a scheduler task for periodic controller updates
     * 
     * @param ctrl Managed controller to add task for
     * @return Task pointer or nullptr on failure
     */
    static ScheduledTask* addControllerTask(ManagedController* ctrl);
    
    /**
     * @brief Remove a scheduler task
     * 
     * @param task Task pointer to remove
     */
    static void removeControllerTask(ScheduledTask* task);
    
    /**
     * @brief Clear controller instance from static array (for task wrappers)
     * 
     * @param index Controller index to clear
     */
    static void clearControllerInstance(uint8_t index);
    
    /**
     * @brief Validate controller configuration parameters
     * 
     * @param config Controller configuration to validate
     * @return true if configuration is valid
     */
    static bool validateConfig(const IPC_ConfigTempController_t* config);
    
    /**
     * @brief Map controller index to array index
     * 
     * @param index Controller index (40-42)
     * @return Array index (0-2) or -1 if invalid
     */
    static int indexToArrayIndex(uint8_t index);
};

// External reference to task scheduler
extern TaskScheduler tasks;
