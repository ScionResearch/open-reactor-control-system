#pragma once

#include <Arduino.h>
#include "sys_init.h"

// Forward declarations
class ScheduledTask;
class TaskScheduler;
class pHController;
class FlowController;
class DOController;

// IPC structures
struct IPC_ConfigpHController_t;
struct IPC_ConfigFlowController_t;
struct IPC_ConfigDOController_t;

// Maximum number of temperature controllers (matches ioConfig.h)
#define MAX_TEMP_CONTROLLERS 3
// Maximum number of flow controllers (3 feed + 1 waste = 4)
#define MAX_FLOW_CONTROLLERS 4

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
 * @brief Managed pH Controller Entry
 * 
 * Tracks all information needed for the pH controller (index 43)
 */
struct ManagedpHController {
    uint8_t index;                          // Controller object index (always 43)
    pHController* controllerInstance;       // Pointer to pH controller class instance
    pHControl_t* controlObject;             // Pointer to control structure in objIndex
    ScheduledTask* updateTask;              // Scheduler task for periodic update()
    bool active;                            // Controller is active and updating
    char message[100];                      // Status/error message
};

/**
 * @brief Managed flow controller structure
 * 
 * Tracks all information needed for flow controllers (indices 44-47)
 */
struct ManagedFlowController {
    uint8_t index;                          // Controller object index (44-47)
    FlowController* controllerInstance;     // Pointer to flow controller class instance
    FlowControl_t* controlObject;           // Pointer to control structure in objIndex
    ScheduledTask* task;                    // Pointer to scheduler task
    bool active;                            // Controller is active
};

/**
 * @brief Managed DO controller structure
 * 
 * Tracks all information needed for the DO controller (index 48)
 */
struct ManagedDOController {
    uint8_t index;                          // Controller object index (always 48)
    DOController* controllerInstance;       // Pointer to DO controller class instance
    DissolvedOxygenControl_t* controlObject; // Pointer to control structure in objIndex
    ScheduledTask* task;                    // Pointer to scheduler task
    bool active;                            // Controller is active
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
    // pH Controller Lifecycle (Index 43)
    // ========================================================================
    
    /**
     * @brief Create a new pH controller instance
     * 
     * @param config Controller configuration from IPC
     * @return true if controller created successfully
     */
    static bool createpHController(const IPC_ConfigpHController_t* config);
    
    /**
     * @brief Delete the pH controller instance
     * 
     * @return true if controller deleted successfully
     */
    static bool deletepHController();
    
    /**
     * @brief Update pH controller configuration
     * 
     * @param config New controller configuration
     * @return true if configuration updated successfully
     */
    static bool configurepHController(const IPC_ConfigpHController_t* config);
    
    // ========================================================================
    // pH Controller Control Commands
    // ========================================================================
    
    /**
     * @brief Set pH controller setpoint
     * 
     * @param setpoint New target pH
     * @return true if setpoint updated successfully
     */
    static bool setpHSetpoint(float setpoint);
    
    /**
     * @brief Enable pH controller
     * 
     * @return true if controller enabled successfully
     */
    static bool enablepHController();
    
    /**
     * @brief Disable pH controller
     * 
     * @return true if controller disabled successfully
     */
    static bool disablepHController();
    
    /**
     * @brief Manual acid dose
     * 
     * @return true if dose started successfully
     */
    static bool dosepHAcid();
    
    /**
     * @brief Manual alkaline dose
     * 
     * @return true if dose started successfully
     */
    static bool dosepHAlkaline();
    
    /**
     * @brief Reset acid cumulative volume to zero
     * 
     * @return true if successful
     */
    static bool resetpHAcidVolume();
    
    /**
     * @brief Reset alkaline cumulative volume to zero
     * 
     * @return true if successful
     */
    static bool resetpHAlkalineVolume();
    
    // ========================================================================
    // Flow Controller Lifecycle (Indices 44-47)
    // ========================================================================
    
    /**
     * @brief Create a new flow controller instance
     * 
     * @param index Controller index (44-47)
     * @param config Controller configuration from IPC
     * @return true if controller created successfully
     */
    static bool createFlowController(uint8_t index, const IPC_ConfigFlowController_t* config);
    
    /**
     * @brief Delete a flow controller instance
     * 
     * @param index Controller index (44-47)
     * @return true if controller deleted successfully
     */
    static bool deleteFlowController(uint8_t index);
    
    /**
     * @brief Update flow controller configuration
     * 
     * @param index Controller index (44-47)
     * @param config New controller configuration
     * @return true if configuration updated successfully
     */
    static bool configureFlowController(uint8_t index, const IPC_ConfigFlowController_t* config);
    
    // ========================================================================
    // Flow Controller Control Commands
    // ========================================================================
    
    /**
     * @brief Set flow controller flow rate setpoint
     * 
     * @param index Controller index (44-47)
     * @param flowRate_mL_min New target flow rate in mL/min
     * @return true if flow rate updated successfully
     */
    static bool setFlowRate(uint8_t index, float flowRate_mL_min);
    
    /**
     * @brief Enable flow controller
     * 
     * @param index Controller index (44-47)
     * @return true if controller enabled successfully
     */
    static bool enableFlowController(uint8_t index);
    
    /**
     * @brief Disable flow controller
     * 
     * @param index Controller index (44-47)
     * @return true if controller disabled successfully
     */
    static bool disableFlowController(uint8_t index);
    
    /**
     * @brief Manual dose (one cycle)
     * 
     * @param index Controller index (44-47)
     * @return true if dose started successfully
     */
    static bool manualFlowDose(uint8_t index);
    
    /**
     * @brief Reset cumulative volume to zero
     * 
     * @param index Controller index (44-47)
     * @return true if successful
     */
    static bool resetFlowVolume(uint8_t index);
    
    /**
     * @brief Find a managed flow controller by its index
     * 
     * @param index Controller index (44-47)
     * @return Pointer to ManagedFlowController or nullptr if not found
     */
    static ManagedFlowController* findFlowController(uint8_t index);
    
    // ========================================================================
    // DO CONTROLLER LIFECYCLE (Index 48)
    // ========================================================================
    
    /**
     * @brief Create or update the DO controller instance
     * 
     * @param config Controller configuration from IPC
     * @return true if controller created successfully
     */
    static bool createDOController(const IPC_ConfigDOController_t* config);
    
    /**
     * @brief Delete the DO controller instance
     * @return true if controller deleted successfully
     */
    static bool deleteDOController();
    
    /**
     * @brief Set DO setpoint
     * 
     * @param setpoint_mg_L Target DO in mg/L
     * @return true if successful
     */
    static bool setDOSetpoint(float setpoint_mg_L);
    
    /**
     * @brief Enable DO controller
     * @return true if successful
     */
    static bool enableDOController();
    
    /**
     * @brief Disable DO controller
     * @return true if successful
     */
    static bool disableDOController();
    
    /**
     * @brief Find the managed DO controller
     * @return Pointer to ManagedDOController or nullptr if not active
     */
    static ManagedDOController* findDOController();
    
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
    static ManagedpHController phController;  // Single pH controller (index 43)
    static ManagedFlowController flowControllers[MAX_FLOW_CONTROLLERS];  // Flow controller array (4 slots)
    static ManagedDOController doController;  // Single DO controller (index 48)
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
