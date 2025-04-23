#pragma once

#include <stdint.h> // For standard integer types

// Structure for PID controller parameters
typedef struct {
    float kp;
    float ki;
    float kd;
    float integral_min; // Anti-windup limits
    float integral_max;
    float output_min;   // Overall PID output limits
    float output_max;
} PID_Params_t;

// Structure for configuring a single cascade parameter
typedef struct {
    float cv_threshold_min; // Primary CV value to start activating this parameter
    float cv_threshold_max; // Primary CV value when this parameter reaches its max output
    float op_range_min;     // Minimum physical operating value (e.g., RPM, LPM, %)
    float op_range_max;     // Maximum physical operating value
} Cascade_Param_Config_t;

// Structure to hold the complete DO control configuration
typedef struct {
    PID_Params_t do_pid_params;
    Cascade_Param_Config_t stir_config;
    Cascade_Param_Config_t gas_flow_config;
    Cascade_Param_Config_t o2_conc_config;
    float sample_time_s; // Control loop sample time in seconds
} DO_Control_Config_t;

// Structure to hold the internal state of the DO controller
typedef struct {
    float integral_term;
    float prev_error;
    float prev_measurement; // For derivative on measurement
    bool initialized;
} PID_State_t;

// Structure to hold the calculated outputs
typedef struct {
    float stir_output;      // Calculated stirrer speed (e.g., RPM)
    float gas_flow_output;  // Calculated gas flow rate (e.g., LPM)
    float o2_conc_output;   // Calculated O2 concentration (e.g., %)
    float primary_cv;       // The raw output from the primary DO PID
} DO_Control_Outputs_t;


// --- Function Prototypes ---

/**
 * @brief Initializes the DO control system with default or provided settings.
 * @param config Pointer to the configuration settings to use.
 */
void do_control_init(const DO_Control_Config_t* config);

/**
 * @brief Updates the configuration of the DO control system.
 * @param config Pointer to the new configuration settings.
 */
void do_control_update_settings(const DO_Control_Config_t* config);

/**
 * @brief Executes one iteration of the DO control loop.
 * @param do_setpoint The desired Dissolved Oxygen value.
 * @param do_measurement The current measured Dissolved Oxygen value.
 * @param outputs Pointer to a structure where the calculated outputs will be stored.
 */
void do_control_run(float do_setpoint, float do_measurement, DO_Control_Outputs_t* outputs);

/**
 * @brief Resets the internal state of the PID controller (e.g., integral term).
 */
void do_control_reset_pid(void);

/**
 * @brief Gets the current configuration being used by the controller.
 * @param config Pointer to a structure where the current config will be copied.
 */
void do_control_get_config(DO_Control_Config_t* config);

/**
 * @brief Gets the current calculated control outputs.
 * @param outputs Pointer to a structure where the current outputs will be copied.
 */
void do_control_get_outputs(DO_Control_Outputs_t* outputs);

