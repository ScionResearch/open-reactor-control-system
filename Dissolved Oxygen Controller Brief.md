# Dissolved Oxygen Controller Brief

## Overview
The DO controller should follow the same architecture as the existing system controllers including Temperature, pH and Flow controllers already implemented. The purpose of the DO controller is to regulate the dissolved oxygen level in the reactor to a setpoint value. While this is a form of closed loop control, requiring a DO sensor, it is not a PID controller. Control is managed by way of either a gas/air supply via mass flow controller, or stirrer driven by a stepper or DC motor, or both of these. The control values for the stirrer and the MFC are derived from a profile created by the user which defines a series of points to create a curve of control values for both (and/or) stirrer speed and flow rates. The point in the curve is derived from the error value (setpoint - DO sensor reading). The curve is linearly interpolated to find the control values for the stirrer and the MFC.

The required configuration for the DO controller is as follows:
- Name
- Show on Dashboard checkbox
- DO sensor
- Mass flow controller (optional)
- Stirrer motor/stepper (optional) must have either MFC or stirrer or both
- Setpoint
- Profile

The profiles should be managed in a separate configuration modal, with the controller card having a select box with the currently selected profile and buttons for adding, deleting and editing profiles. The profile add/config modal should allow the user to add a series of points to create a curve of control values for both (and/or) stirrer speed and flow rates. The points should be defined by the user as a series of x,y pairs, where x is the error value (setpoint - DO sensor reading) and y is the control value (stirrer speed and/or MFC flow rate). We should also draw and simple chart showing the curve of control values for both (and/or) stirrer speed and flow rates with gas flow on y1 and rpm/% power on y2. Both the x and y values should be editable. Once created a profile can be selected from the controller card and activated - which sends the profile to the io mcu controller object. In this way multiple profiles can be created and stored on the larger sys mcu flash, only the active profile is sent to the io mcu controller object.

Note that the profile should allow for x values in mg/L, and y values should be rpm or % for stirrer, and in user selectable units for the flow rate (mL/min as default). We will need to check the profile flow units against the MFC set units and convert if necessary - this should be handled in the MFC device driver (we will need to add this feature).