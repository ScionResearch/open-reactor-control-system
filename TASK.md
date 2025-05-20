# Open Reactor Control System - Project Tasks

## 📋 Active Tasks
<!-- Current tasks being worked on -->
- [ ] Finalize and review project documentation
- [ ] Implement the TMC5130A SPI stepper motor interface for the I/O Controller
- [ ] Develop DRV8235 I2C DC motor driver interface for the I/O Controller
- [ ] Create thermal control component for temperature regulation
- [ ] Begin development of pH control component
- [ ] Implement slow PWM for heating control
- [ ] Develop GPIO access drivers
- [ ] Design web interface dashboard with live sensor data

## 🗓️ IO Controller Backlog (SAMD51)
<!-- Future tasks planned for IO Controller but not yet started -->
### Driver Development (On-board Devices)
- [ ] Stepper motor driver implementation
- [ ] DC motors control interface
- [ ] Open drain outputs driver
- [ ] High current output driver
- [ ] GPIO access implementation
- [ ] Slow PWM for heating control

### Driver Development (External Devices)
- [ ] Hamilton Optical Density sensor integration
- [ ] Mass flow controller interface and driver
- [ ] Backpressure controller interface

### Control Components
- [ ] Thermal control component
- [ ] pH control component
- [ ] Feed control component
- [ ] Waste control component
- [ ] Global control component (Run/Pause/Stop)
- [ ] Safety manager component

## 🗓️ System Controller Backlog (RP2040)
<!-- Future tasks planned for System Controller but not yet started -->
### System Components
- [ ] Realtime clock interface and time management
- [ ] Status manager and LED driver
- [ ] Main supplies voltage monitoring
- [ ] Terminal for debug (interactive)
- [ ] Unified and thread-safe terminal access for debug
- [ ] Response for various terminal user requests

### Storage Management
- [ ] SD card SDIO interface
- [ ] Logging component
- [ ] System event log management
- [ ] Operational event log management
- [ ] Sensor recording management

### Network Components
- [ ] W5500 ethernet interface
- [ ] Network management

### Web Interface
- [ ] Dashboard with live sensor data
- [ ] Simple chart with core metrics
- [ ] Basic controls
- [ ] System configuration
- [ ] Driver configuration
- [ ] Port configuration
- [ ] Sensor configuration and assignment
- [ ] Calibration interface
- [ ] Local File Manager with SD contents browser
- [ ] Download small files
- [ ] System Status display (power supplies, clock, communications, SD card)
- [ ] Settings (Time and NTP, IP, MQTT)

### Communication
- [ ] MQTT interface
- [ ] IPC structure design optimization

## ✅ Completed Tasks
<!-- Tasks that have been completed -->
### IO Controller
- [x] Object structure definitions (objects.h)
- [x] MCP48FEBxx SPI DAC interface
- [x] MCP346x SPI ΣΔ ADC interface
- [x] MAX31865 SPI RTD interface
- [x] INA260 I2C power analyzer interface
- [x] RS485/232 Modbus interface
- [x] Analogue outputs implementation
- [x] Analogue inputs implementation
- [x] RTD sensors integration
- [x] Power monitoring implementation
- [x] Hamilton DO sensor integration
- [x] Hamilton pH sensor integration
- [x] DO control component
- [x] Inter-processor communication implementation

### System Controller
- [x] Initial project setup
- [x] Project structure creation
- [x] Inter-processor communication implementation

## 🔍 Discovered During Development
<!-- Tasks or issues discovered while working on other tasks -->
- [ ] Need to research better PID tuning methodology for temperature control
- [ ] Investigate potential memory optimization for SAMD51 firmware
- [ ] Document communication protocol between MCUs for future maintenance
- [ ] Research failsafe mechanisms for safety-critical control functions
- [ ] Consider adding support for additional bioreactor sensors
- [ ] Evaluate power consumption and potential optimizations

## 🚧 Blocked Tasks
<!-- Tasks blocked by dependencies or issues -->
- [ ] Web interface implementation (Blocked: Waiting for W5500 ethernet interface)
- [ ] MQTT communication (Blocked: Requires network implementation)
- [ ] Sensor data visualization (Blocked: Waiting for logging implementation)

## 🏁 Milestones
<!-- Key project milestones -->
- [x] Initial hardware design and PCB assembly
- [x] Basic communication between System and I/O Controllers
- [x] Core sensor interfaces implementation
- [ ] Complete I/O Controller functionality
- [ ] Complete System Controller functionality
- [ ] Network and web interface implementation
- [ ] Full system integration and testing
- [ ] Initial deployment and field testing
- [ ] Documentation and knowledge transfer

## 📊 Testing & Validation Tasks
<!-- Specific tasks related to testing and validation -->
- [ ] Create comprehensive unit test framework
- [ ] Develop hardware-in-the-loop testing system
- [ ] Implement automated regression testing
- [ ] Perform environmental testing (temperature, humidity)
- [ ] Validate all control algorithms
- [ ] Test failsafe mechanisms and error handling
- [ ] Verify real-time performance under various loads
- [ ] Conduct long-term stability testing