# DO Cascade Control Module (`orc-io-mcu`)

This module implements a cascade control strategy for regulating Dissolved Oxygen (DO) within the Open Reactor system. The control logic resides entirely within the `orc-io-mcu` firmware.

## Functionality

1.  **Primary DO PID Controller:** A standard Proportional-Integral-Derivative (PID) controller calculates a primary control variable (CV) based on the DO setpoint and the measured DO value.
2.  **Cascade Parameters:** The primary CV drives three subsequent control parameters in a cascade manner:
    *   Stirring Speed (e.g., RPM)
    *   Gas Flow Rate (e.g., LPM)
    *   Oxygen Concentration in Gas (%)
3.  **Configurable Ranges:** Each cascade parameter has two configurable ranges:
    *   **CV Threshold Range:** Defines the segment of the primary CV (min/max) over which this parameter is active.
    *   **Operating Range:** Defines the minimum and maximum physical output limits for the parameter (e.g., 400-1200 RPM).
4.  **Scaling:** The output for each parameter is scaled proportionally based on the primary CV's position within its assigned threshold range, mapping it to the corresponding operating range. This ensures smooth transitions as the primary CV increases and activates subsequent parameters.
5.  **Configuration:** Data structures (`DO_Control_Config_t`, `PID_Params_t`, `Cascade_Param_Config_t`) are defined in `do_control.h` to hold all necessary settings (PID gains, anti-windup limits, CV thresholds, operating ranges, sample time).
6.  **Interface:**
    *   `do_control_init()`: Initializes the controller with configuration settings.
    *   `do_control_update_settings()`: Updates the configuration dynamically.
    *   `do_control_run()`: Executes a single control loop iteration, taking the DO setpoint and measurement, and calculating the outputs for stirring, gas flow, and O2 concentration.
    *   `do_control_reset_pid()`: Resets the PID controller's internal state.
    *   `do_control_get_config()`: Retrieves the current configuration.
    *   `do_control_get_outputs()`: Retrieves the latest calculated outputs.

## Files

*   `do_control.h`: Contains the public interface, including data structure definitions and function prototypes.
*   `do_control.cpp`: Contains the private implementation details, including static variables for state and configuration, helper functions for scaling and constraining values, and the implementation of the public functions.

## Usage

1.  Initialize the controller using `do_control_init()` during system startup, providing the desired configuration.
2.  Periodically call `do_control_run()` with the current DO setpoint and measurement.
3.  Retrieve the calculated outputs using `do_control_get_outputs()` or directly from the structure passed to `do_control_run()`.
4.  Use these outputs to command the respective hardware drivers (stepper motor, DACs, etc.).
