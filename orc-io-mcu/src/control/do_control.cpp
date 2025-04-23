#include "do_control.h"
#include <math.h>   // For isnan, isinf, fabs
#include <string.h> // For memcpy
#include <algorithm> // For std::min, std::max

// --- Private Variables ---

static DO_Control_Config_t current_config;
static PID_State_t pid_state;
static DO_Control_Outputs_t current_outputs;
static bool control_initialized = false;

// Helper function to constrain a value within limits
static inline float constrain(float value, float min_val, float max_val) {
    return std::min(std::max(value, min_val), max_val);
}

// Helper function to scale a value from one range to another
static inline float scale_value(float value, float in_min, float in_max, float out_min, float out_max) {
    // Avoid division by zero
    if (fabs(in_max - in_min) < 1e-6) {
        return out_min; // Or handle as an error, return midpoint, etc.
    }
    // Constrain input value to its range first
    value = constrain(value, in_min, in_max);
    // Calculate proportion and scale
    float proportion = (value - in_min) / (in_max - in_min);
    return out_min + proportion * (out_max - out_min);
}


// --- Public Function Implementations ---

void do_control_init(const DO_Control_Config_t* config) {
    if (config != nullptr) {
        memcpy(&current_config, config, sizeof(DO_Control_Config_t));
    } else {
        // Initialize with some safe defaults if no config provided
        // TODO: Define sensible default values
        memset(&current_config, 0, sizeof(DO_Control_Config_t));
        current_config.sample_time_s = 1.0f; // Default 1 second sample time
        // Set default PID gains (example P-only)
        current_config.do_pid_params.kp = 1.0f;
        current_config.do_pid_params.output_min = 0.0f;
        current_config.do_pid_params.output_max = 100.0f; // Example CV range 0-100
        current_config.do_pid_params.integral_min = 0.0f;
        current_config.do_pid_params.integral_max = 100.0f;
        // Set default cascade params (example ranges)
        current_config.stir_config = {0.0f, 20.0f, 400.0f, 1200.0f};
        current_config.gas_flow_config = {20.0f, 60.0f, 0.5f, 5.0f};
        current_config.o2_conc_config = {60.0f, 100.0f, 21.0f, 100.0f}; // Assuming % O2
    }
    do_control_reset_pid();
    memset(&current_outputs, 0, sizeof(DO_Control_Outputs_t));
    control_initialized = true;
}

void do_control_update_settings(const DO_Control_Config_t* config) {
    if (config != nullptr) {
        memcpy(&current_config, config, sizeof(DO_Control_Config_t));
        // Consider if PID state needs reset upon config change
        // do_control_reset_pid();
    }
}

void do_control_reset_pid(void) {
    pid_state.integral_term = 0.0f;
    pid_state.prev_error = 0.0f;
    pid_state.prev_measurement = NAN; // Use NAN to indicate first run
    pid_state.initialized = true;
}

void do_control_run(float do_setpoint, float do_measurement, DO_Control_Outputs_t* outputs) {
    if (!control_initialized || !pid_state.initialized) {
        // Handle error: Controller not initialized
        if (outputs != nullptr) {
            memset(outputs, 0, sizeof(DO_Control_Outputs_t)); // Zero outputs
        }
        return;
    }

    // --- Input Validation ---
    if (isnan(do_setpoint) || isinf(do_setpoint) || isnan(do_measurement) || isinf(do_measurement)) {
       // Handle invalid inputs - maybe hold last outputs or go to safe state?
       // For now, just return without updating.
       if (outputs != nullptr) {
           memcpy(outputs, &current_outputs, sizeof(DO_Control_Outputs_t));
       }
       return;
    }


    // --- PID Calculation ---
    float error = do_setpoint - do_measurement;
    float p_term = current_config.do_pid_params.kp * error;

    // Integral Term (with anti-windup)
    pid_state.integral_term += current_config.do_pid_params.ki * error * current_config.sample_time_s;
    pid_state.integral_term = constrain(pid_state.integral_term,
                                        current_config.do_pid_params.integral_min,
                                        current_config.do_pid_params.integral_max);
    float i_term = pid_state.integral_term;

    // Derivative Term (using derivative on measurement to avoid derivative kick)
    float d_term = 0.0f;
    if (!isnan(pid_state.prev_measurement) && current_config.sample_time_s > 1e-6) {
         // Check sample_time_s to avoid division by zero
        float measurement_derivative = (do_measurement - pid_state.prev_measurement) / current_config.sample_time_s;
        d_term = -current_config.do_pid_params.kd * measurement_derivative; // Note the negative sign
    }

    // --- Calculate Primary Control Variable (CV) ---
    float primary_cv = p_term + i_term + d_term;
    primary_cv = constrain(primary_cv,
                           current_config.do_pid_params.output_min,
                           current_config.do_pid_params.output_max);

    // --- Update PID State for next iteration ---
    pid_state.prev_error = error;
    pid_state.prev_measurement = do_measurement;

    // --- Cascade Logic ---

    // 1. Stirring Speed
    current_outputs.stir_output = scale_value(primary_cv,
                                             current_config.stir_config.cv_threshold_min,
                                             current_config.stir_config.cv_threshold_max,
                                             current_config.stir_config.op_range_min,
                                             current_config.stir_config.op_range_max);

    // 2. Gas Flow Rate
    current_outputs.gas_flow_output = scale_value(primary_cv,
                                                 current_config.gas_flow_config.cv_threshold_min,
                                                 current_config.gas_flow_config.cv_threshold_max,
                                                 current_config.gas_flow_config.op_range_min,
                                                 current_config.gas_flow_config.op_range_max);

    // 3. O2 Concentration
    current_outputs.o2_conc_output = scale_value(primary_cv,
                                                current_config.o2_conc_config.cv_threshold_min,
                                                current_config.o2_conc_config.cv_threshold_max,
                                                current_config.o2_conc_config.op_range_min,
                                                current_config.o2_conc_config.op_range_max);

    // Store the primary CV as well
    current_outputs.primary_cv = primary_cv;


    // --- Output ---
    if (outputs != nullptr) {
        memcpy(outputs, &current_outputs, sizeof(DO_Control_Outputs_t));
    }
}

void do_control_get_config(DO_Control_Config_t* config) {
    if (config != nullptr && control_initialized) {
        memcpy(config, &current_config, sizeof(DO_Control_Config_t));
    } else if (config != nullptr) {
        // Optionally clear the passed struct if not initialized
         memset(config, 0, sizeof(DO_Control_Config_t));
    }
}

void do_control_get_outputs(DO_Control_Outputs_t* outputs) {
     if (outputs != nullptr && control_initialized) {
        memcpy(outputs, &current_outputs, sizeof(DO_Control_Outputs_t));
    } else if (outputs != nullptr) {
        // Optionally clear the passed struct if not initialized
         memset(outputs, 0, sizeof(DO_Control_Outputs_t));
    }
}

