#include "do_control.h"
#include <math.h>   // For isnan, isinf
#include <string.h> // For memset
#include <algorithm> // For std::min, std::max

// --- Private Variables ---

// Internal state for the PID controller
static PID_State_t pid_state;

// Keep track of the last calculated outputs
static DO_Control_Outputs_t current_outputs;

// --- Helper Functions ---

// Helper function to constrain a value within limits
static inline float constrain(float value, float min_val, float max_val) {
    return std::min(std::max(value, min_val), max_val);
}

// Helper function to map the primary PID CV to a specific actuator output
// based on its cascade configuration.
static float calculate_cascade_output(float primary_cv, const Cascade_Param_Config_t* config) {
    if (config == nullptr) return 0.0f; // Safety check

    // Constrain primary_cv to the threshold range for interpolation
    float constrained_cv = constrain(primary_cv, config->cv_threshold_min, config->cv_threshold_max);

    // Check if the CV is below the minimum threshold for this parameter
    if (constrained_cv <= config->cv_threshold_min) {
        return config->op_range_min;
    }

    // Check if the CV is above the maximum threshold for this parameter
    if (constrained_cv >= config->cv_threshold_max) {
        return config->op_range_max;
    }

    // Calculate the proportion within the active CV range
    float cv_range = config->cv_threshold_max - config->cv_threshold_min;
    float proportion = 0.0f;
    if (fabs(cv_range) > 1e-6) { // Avoid division by zero
        proportion = (constrained_cv - config->cv_threshold_min) / cv_range;
    }

    // Interpolate the output within the operational range
    float op_range = config->op_range_max - config->op_range_min;
    return config->op_range_min + proportion * op_range;
}

// --- Public Function Implementations ---

void do_control_init(const DissolvedOxygenControl_t* control_object) {
    // Initialize PID state
    memset(&pid_state, 0, sizeof(PID_State_t));
    pid_state.initialized = true;

    // Initialize outputs to zero
    memset(&current_outputs, 0, sizeof(DO_Control_Outputs_t));

    // Note: Configuration (PID params, cascade settings) is taken from
    // control_object when do_control_run is called.
}

void do_control_reset_pid(void) {
    // Reset integral term and previous error/measurement
    pid_state.integral_term = 0.0f;
    pid_state.prev_error = 0.0f;
    pid_state.prev_measurement = 0.0f; // Or consider setting to current measurement if available
    // Keep initialized flag true
}

void do_control_run(const DissolvedOxygenControl_t* control_object, DO_Control_Outputs_t* outputs) {
    if (!pid_state.initialized || control_object == nullptr || outputs == nullptr) {
        // Handle error: Controller not initialized or invalid pointers
        if (outputs != nullptr) {
            memset(outputs, 0, sizeof(DO_Control_Outputs_t)); // Zero outputs
        }
        return;
    }

    // Check if control is enabled
    if (!control_object->enabled) {
        // Control is disabled, set outputs to zero (or a safe state)
        current_outputs.primary_cv = 0.0f;
        current_outputs.stir_output = 0.0f;
        current_outputs.gas_flow_output = 0.0f;
        current_outputs.o2_conc_output = 0.0f;
        do_control_reset_pid(); // Reset PID state when disabled
        memcpy(outputs, &current_outputs, sizeof(DO_Control_Outputs_t));
        return;
    }

    // --- Input Validation ---
    float do_measurement = control_object->sensor.dissolvedOxygen;
    float do_setpoint = control_object->setpoint;
    const PID_Params_t* pid_params = &control_object->pid_params;
    float sample_time = control_object->sample_time_s;

    if (isnan(do_measurement) || isinf(do_measurement) || control_object->sensor.fault || sample_time <= 0.0f) {
       // Handle invalid measurement, sensor fault, or invalid sample time
       // Hold last valid outputs (or go to a defined safe state)
       memcpy(outputs, &current_outputs, sizeof(DO_Control_Outputs_t));
       // Optionally reset PID here too, or just prevent integration windup
       pid_state.integral_term = constrain(pid_state.integral_term, pid_params->integral_min, pid_params->integral_max);
       return;
    }

    // --- PID Calculation ---
    float error = do_setpoint - do_measurement;

    // Proportional term
    float p_term = pid_params->kp * error;

    // Integral term with anti-windup
    pid_state.integral_term += pid_params->ki * error * sample_time;
    pid_state.integral_term = constrain(pid_state.integral_term, pid_params->integral_min, pid_params->integral_max);
    float i_term = pid_state.integral_term;

    // Derivative term (on measurement to reduce setpoint kick)
    float derivative = (do_measurement - pid_state.prev_measurement) / sample_time;
    float d_term = -pid_params->kd * derivative; // Negative because it's on measurement

    // Combine terms for primary control variable (CV)
    float primary_cv = p_term + i_term + d_term;

    // Constrain overall PID output
    primary_cv = constrain(primary_cv, pid_params->output_min, pid_params->output_max);

    // Update state for next iteration
    pid_state.prev_error = error;
    pid_state.prev_measurement = do_measurement;

    // --- Cascade Logic --- 
    // Calculate individual actuator outputs based on the primary CV and cascade configs
    current_outputs.stir_output = calculate_cascade_output(primary_cv, &control_object->stir_config);
    current_outputs.gas_flow_output = calculate_cascade_output(primary_cv, &control_object->gas_flow_config);
    current_outputs.o2_conc_output = calculate_cascade_output(primary_cv, &control_object->o2_conc_config);
    current_outputs.primary_cv = primary_cv; // Store the raw PID output as well

    // --- Output ---
    memcpy(outputs, &current_outputs, sizeof(DO_Control_Outputs_t));
}

void do_control_get_outputs(DO_Control_Outputs_t* outputs) {
    if (outputs != nullptr) {
        memcpy(outputs, &current_outputs, sizeof(DO_Control_Outputs_t));
    }
}

