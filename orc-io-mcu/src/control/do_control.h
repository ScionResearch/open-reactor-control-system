#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "drivers/objects.h" // Include the definition of DissolvedOxygenControl_t


// --- Internal State & Output Structs (Specific to this implementation) ---

// Structure to hold the internal state of the PID controller
typedef struct {
    float integral_term;
    float prev_error;
    float prev_measurement; // For derivative on measurement
    bool initialized;
} PID_State_t;

// Structure to hold the calculated outputs from the cascade logic
typedef struct {
    float stir_output;      // Calculated stirrer speed (e.g., RPM)
    float gas_flow_output;  // Calculated gas flow rate (e.g., LPM)
    float o2_conc_output;   // Calculated O2 concentration (e.g., %)
    float primary_cv;       // The raw output from the primary DO PID
} DO_Control_Outputs_t;


// --- Function Prototypes ---

/**
 * @brief Initializes the DO control system's internal state.
 * @param control_object Pointer to the DO control object containing configuration.
 *                     (Note: Configuration itself is assumed to be set externally).
 */
void do_control_init(const DissolvedOxygenControl_t* control_object);

/**
 * @brief Executes one iteration of the DO PID + Cascade control loop.
 * @param control_object Pointer to the DO control object containing sensor data, setpoint, and PID/Cascade config.
 * @param outputs Pointer to a structure where the calculated actuator outputs will be stored.
 */
void do_control_run(const DissolvedOxygenControl_t* control_object, DO_Control_Outputs_t* outputs);

/**
 * @brief Resets the internal state of the PID controller (e.g., integral term).
 */
void do_control_reset_pid(void);

/**
 * @brief Gets the last calculated control outputs.
 * @param outputs Pointer to a structure where the current outputs will be copied.
 */
void do_control_get_outputs(DO_Control_Outputs_t* outputs);

