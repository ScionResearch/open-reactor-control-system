# Project tasks

Project task list to help keep track of things!

## System Interface Firmware

System interface firmware lives on the RP2040 MCU flash storage and includes the backend C++ firmware, and the frontend html, css and js. The system handles local logging, system configuration, network connection and external interface via MQTT.

### Checklist

High level todo items

- [X] Realtime clock interface and time management
- [X] Status manager and LED driver
- [X] Main supplies voltage monitoring
- [X] Terminal for debug (interactive)

  - [X] Unified and thread-safe terminal access for debug
  - [X] Response for various terminal user requests
- [X] SD card SDIO interface
- [ ] Logging component

  - [X] System event log management
  - [ ] Operational event log management
  - [ ] Sensor recoding management
- [ ] Inter-processor communication

  - [X] IPC structure design
  - [ ] IPC implementation
- [X] W5500 ethernet interface
- [X] Network management
- [ ] Web interface development

  - [ ] Dashboard
    - [ ] Sensor data live
    - [ ] Simple chart with core metrics
    - [ ] Basic controls
  - [ ] System Config
    - [ ] Driver configuration
    - [ ] Port configuration
  - [ ] Sensor Config
    - [ ] Sensor assignment
    - [ ] Calibration
  - [X] Local File Manager
    - [X] Local browser view of SD contents
    - [X] Download small files
  - [ ] System Status
    - [X] Power supplies
    - [X] Clock
    - [ ] Comms - IPC, MQTT and Modbus
    - [X] SD card status and info
  - [X] Settings
    - [X] Time and NTP config
    - [X] IP setting
    - [X] MQTT parameters
- [ ] MQTT interface

  - [ ] Object planning
  - [ ] Implementation and testing

## IO Interface Firmware

IO interface firmware lives on the ATSAME51N20A MCU and includes the core operational programs, object definitions, device drivers and sensor connections. The IO interface handles all of the device connections and control, including on-board interfaces such as motor and stepper controls, high resolution analogue inputs, RTD interfaces, analogue outputs and simple IO, RS485 and RS232 data busses.

### Checklist

High level todo items

- [X] Object structure definitios objects.h
- [ ] On-board hardware interface libraries

  - [X] MCP48FEBxx SPI DAC interface dev
  - [X] MCP346x SPI ΣΔ ADC interface dev
  - [X] MAX31865 SPI RTD interface dev
  - [ ] TMC5130A SPI stepper interface dev
  - [ ] DRV8235 I2C DC motor driver interface dev
  - [ ] INA260 I2C power analyser interface dev
  - [ ] RS485/232 Modbus interface dev
  - [ ] Slow PWM for heating control high current output
- [ ] Driver development - on-board devices

  - [X] Analogue outputs
  - [X] Analogue inputs
  - [X] RTD sensors
  - [ ] GPIO access
  - [ ] Stepper motor
  - [ ] DC motors
  - [ ] Open drain outputs
  - [ ] High current output
  - [ ] Power monitoring
- [ ] Driver development - external devices

  - [ ] Hamilton DO sensor
  - [ ] Hamilton pH sensor
  - [ ] Hamilton Optical Density sensor
  - [ ] Mass flow controller
  - [ ] Backpressure controller
- [ ] Core control program dev

  - [ ] Thermal control component
  - [ ] pH control component
  - [ ] DO control component
  - [ ] Feed control component
  - [ ] Waste control component
  - [ ] Global control component (Run/Pause/Stop?)
  - [ ] Safety manager component
- [ ] Inter-processor communication implementation
