# DO Cascade Control Module (`orc-io-mcu`)

This module implements a cascade control strategy for regulating Dissolved Oxygen (DO) within the Open Reactor system. The control logic resides entirely within the `orc-io-mcu` firmware.

## Functionality

1.  **Primary DO PID Controller:** A standard Proportional-Integral-Derivative (PID) controller calculates a primary control variable (CV) based on the DO setpoint and the measured DO value from the associated sensor. It includes anti-windup for the integral term and uses derivative-on-measurement to reduce setpoint kick.
2.  **Cascade Parameters:** The primary CV drives three subsequent control parameters in a cascade manner:
    *   Stirring Speed (e.g., RPM)
    *   Gas Flow Rate (e.g., LPM)
    *   Oxygen Concentration in Gas (%)
3.  **Configurable Ranges:** Each cascade parameter has two configurable ranges defined in its `Cascade_Param_Config_t` structure:
    *   **CV Threshold Range (`cv_threshold_min`, `cv_threshold_max`):** Defines the segment of the primary CV over which this parameter is active.
    *   **Operating Range (`op_range_min`, `op_range_max`):** Defines the minimum and maximum physical output limits for the parameter (e.g., 400-1200 RPM).
4.  **Scaling:** The output for each parameter is scaled linearly based on the primary CV's position within its assigned threshold range, mapping it to the corresponding operating range. This ensures smooth transitions as the primary CV increases and activates subsequent parameters. If the primary CV is outside the threshold range, the output is clamped to the corresponding operating range limit.
5.  **Configuration:** The primary configuration structure is `DissolvedOxygenControl_t` (defined in `drivers/objects.h`). This structure holds:
    *   The associated `DissolvedOxygenSensor_t` object.
    *   The `enabled` flag for the control loop.
    *   The `setpoint` for the DO value.
    *   The `PID_Params_t` structure containing PID gains (Kp, Ki, Kd) and limits (integral min/max, output min/max).
    *   Three `Cascade_Param_Config_t` structures (`stir_config`, `gas_flow_config`, `o2_conc_config`) defining the thresholds and operating ranges for each cascade output.
    *   The `sample_time_s` for the control loop calculations.
    *   Both `PID_Params_t` and `Cascade_Param_Config_t` are also defined in `drivers/objects.h`.
6.  **State Management:** Internal PID state (integral term, previous error, previous measurement) is maintained statically within `do_control.cpp`.
7.  **Interface:**
    *   `do_control_init(const DissolvedOxygenControl_t* control_object)`: Initializes the controller's internal state. Note: It uses the passed object primarily to know *if* configuration exists, but the actual configuration values are read during `do_control_run`.
    *   `do_control_run(const DissolvedOxygenControl_t* control_object, DO_Control_Outputs_t* outputs)`: Executes a single control loop iteration. It reads the current sensor value, setpoint, enabled state, sample time, and all PID/Cascade configuration parameters directly from the provided `control_object`. It calculates the primary CV and the individual cascade outputs, storing them in the `outputs` structure. Handles sensor faults and invalid inputs.
    *   `do_control_reset_pid()`: Resets the PID controller's internal state (integral term, previous values).
    *   `do_control_get_outputs(DO_Control_Outputs_t* outputs)`: Retrieves the latest calculated outputs stored internally.

## Files

*   `src/control/do_control.h`: Contains the public interface (function prototypes) and definitions for internal state (`PID_State_t`) and output (`DO_Control_Outputs_t`) structures. Includes `drivers/objects.h`.
*   `src/control/do_control.cpp`: Contains the private implementation details, including static variables for state, helper functions for scaling and constraining values, and the implementation of the public functions.
*   `src/drivers/objects.h`: Contains the definition of the main configuration structure `DissolvedOxygenControl_t` and its constituent parameter structures `PID_Params_t` and `Cascade_Param_Config_t`.

## Usage

1.  Define and initialize a `DissolvedOxygenControl_t` object elsewhere in the system (e.g., managed by a higher-level control task or configuration manager). Populate its fields with the desired sensor association, setpoint, PID parameters, cascade configurations, and sample time.
2.  Call `do_control_init()` once during system startup, passing a pointer to the (potentially partially initialized) `DissolvedOxygenControl_t` object.
3.  Periodically (ideally matching `sample_time_s`), update the `DissolvedOxygenControl_t` object with the latest sensor readings, setpoint, and enabled state as needed.
4.  Call `do_control_run()`, passing the updated `DissolvedOxygenControl_t` object and a pointer to a `DO_Control_Outputs_t` structure to receive the results.
5.  Use the calculated outputs (`stir_output`, `gas_flow_output`, `o2_conc_output` from the `DO_Control_Outputs_t` structure) to command the respective hardware drivers (stepper motor, DACs, etc.).
6.  Call `do_control_reset_pid()` if needed (e.g., when the control loop is disabled or operating conditions change significantly).
