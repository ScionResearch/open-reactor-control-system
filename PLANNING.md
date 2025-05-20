# Project Planning

## 🚀 Vision & Purpose
The Open Reactor Control System aims to provide a unified control platform for bioreactor systems with robust control and monitoring capabilities. It solves the challenge of reliable, precise bioreactor control by implementing a dual microcontroller architecture that separates user interface/high-level control from real-time sensor monitoring and control functions. The system provides deterministic, real-time monitoring and control of critical bioreactor parameters including dissolved oxygen (DO), pH, temperature, gas flow, and stirring speed, enabling reliable and reproducible fermentation and cell culture operations.

## 🏗️ Architecture
### System Architecture
- **System Controller (RP2040)** ↔️ **Inter-processor Communication (IPC)** ↔️ **I/O Controller (SAMD51)**
  - **System Controller**: Handles user interface, logging, network connections, and high-level control
  - **I/O Controller**: Provides dedicated, real-time I/O processing and deterministic sensor monitoring/control
  - **Inter-processor Communication**: Custom protocol for reliable data exchange between MCUs

### Data Flow
1. Data enters through multiple inputs:
   - Sensors connected to I/O Controller (pH, dissolved oxygen, temperature, etc.)
   - User inputs via web interface on System Controller
   - External commands via MQTT and Modbus interfaces
2. Processed by specialized control components:
   - Real-time control loops in I/O Controller (PID control, cascade control)
   - Data formatting and validation in System Controller
3. Results stored in multiple storage components:
   - SD card for logging events and sensor data
   - EEPROM for configuration data and calibration values
4. Accessed via:
   - Web interface dashboard with real-time data visualization
   - File manager for log downloads
   - MQTT for external data consumption

### AI Integration Points
- AI Assistant Access: Focus on control algorithms (PID/cascade control), sensor interfacing, and communication protocols
- Knowledge Resources: 
  - Sensor modbus maps in docs folder
  - Hardware pin definitions
  - DO control documentation

## 🧩 Tech Stack
### Languages & Platforms
- **C/C++**: Core programming language for both microcontrollers
- **PlatformIO**: Development platform for MCU programming
- **HTML/CSS/JavaScript**: Web interface on the System Controller
- **JSON**: Data format for configuration and IPC

### Hardware Components
- **RP2040**: Dual-core ARM Cortex M0+ System Controller
- **ATSAME51N20A**: ARM Cortex M4F I/O Controller
- **W5500**: Ethernet connectivity
- **MCP48FEBxx**: SPI DAC for analog outputs
- **MCP346x**: SPI ADC for analog inputs
- **MAX31865**: SPI RTD interface for temperature sensing
- **TMC5130A**: SPI stepper motor driver
- **DRV8235**: I2C DC motor driver
- **INA260**: I2C power analyzer
- **MCP79410**: I2C real-time clock

### Libraries & Frameworks
- **ModbusRTUMaster**: For Modbus communication with sensors
- **FlashStorage_SAMD**: For EEPROM operations
- **IPCProtocol**: Custom inter-processor communication
- **Control Components**: PID and Cascade control implementations

### IO Interface Firmware (SAMD51)
IO interface firmware lives on the ATSAME51N20A MCU and includes the core operational programs, object definitions, device drivers and sensor connections. The IO interface handles all of the device connections and control, including on-board interfaces such as motor and stepper controls, high resolution analogue inputs, RTD interfaces, analogue outputs and simple IO, RS485 and RS232 data busses.

#### Core Structure
- [x] Object structure definitions (objects.h)

#### Hardware Interface Libraries
- [x] MCP48FEBxx SPI DAC interface
- [x] MCP346x SPI ΣΔ ADC interface
- [x] MAX31865 SPI RTD interface
- [ ] TMC5130A SPI stepper interface
- [ ] DRV8235 I2C DC motor driver interface
- [x] INA260 I2C power analyzer interface
- [x] RS485/232 Modbus interface
- [ ] Slow PWM for heating control

#### Driver Development (On-board Devices)
- [x] Analogue outputs
- [x] Analogue inputs
- [x] RTD sensors
- [ ] GPIO access
- [ ] Stepper motor
- [ ] DC motors
- [ ] Open drain outputs
- [ ] High current output
- [x] Power monitoring

#### Driver Development (External Devices)
- [x] Hamilton DO sensor
- [x] Hamilton pH sensor
- [ ] Hamilton Optical Density sensor
- [ ] Mass flow controller
- [ ] Backpressure controller

#### Control Components
- [x] DO control component
- [ ] Thermal control component
- [ ] pH control component
- [ ] Feed control component
- [ ] Waste control component
- [ ] Global control component (Run/Pause/Stop)
- [ ] Safety manager component
- [x] Inter-processor communication implementation

### System Interface Firmware (RP2040)
System interface firmware lives on the RP2040 MCU flash storage and includes the backend C++ firmware, and the frontend html, css and js. The system handles local logging, system configuration, network connection and external interface via MQTT.

#### Core Structure
- [x] Inter-processor communication implementation

#### System Components
- [ ] Realtime clock interface and time management
- [ ] Status manager and LED driver
- [ ] Main supplies voltage monitoring
- [ ] Terminal for debug (interactive)
- [ ] Unified and thread-safe terminal access for debug
- [ ] Response for various terminal user requests

#### Storage Management
- [ ] SD card SDIO interface
- [ ] Logging component
- [ ] System event log management
- [ ] Operational event log management
- [ ] Sensor recording management

#### Network Components
- [ ] W5500 ethernet interface
- [ ] Network management

#### Web Interface
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
- [ ] System Status (power supplies, clock, communications, SD card)
- [ ] Settings (Time and NTP, IP, MQTT)

#### Communication
- [ ] MQTT interface
- [ ] IPC structure design

## 🧩 Components
### Core Components
- **Sensor Interface Layer**: Handles direct communication with physical sensors (pH, DO, temperature, pressure, flow) and provides abstraction for hardware differences
- **Control Loop Components**: Implements various control algorithms (PID, cascade) for maintaining setpoints of process variables
- **Inter-processor Communication Protocol**: Facilitates reliable data exchange between the System Controller and I/O Controller
- **Web Interface**: Provides a user-friendly dashboard for monitoring and controlling the bioreactor system
- **Data Logging System**: Records all sensor readings, events, and system states for analysis and troubleshooting
- **Device Drivers**: Low-level interfaces to hardware components (DACs, ADCs, motor drivers, etc.)

### Sensor/Control Objects
- **Sensors**: Power, temperature, pH, dissolved oxygen, optical density, gas flow, pressure, stirrer speed, weight
- **Controls**: Temperature control, pH control, dissolved oxygen control, gas flow control, stirrer speed control, pump speed control, feed control, waste control

## 🔍 Constraints & Requirements
### Technical Constraints
- Limited processing power on microcontrollers (RP2040 and SAMD51)
- Memory constraints on both microcontrollers
- Real-time responsiveness required for critical control functions
- Communication bandwidth limitations between MCUs
- Hardware-specific constraints for each component interface

### Non-functional Requirements
- Performance: Control loops must maintain deterministic timing for reliable control
- Security: Protection against unauthorized access and data integrity
- Scalability: Modular design allowing for additional sensors/controls
- Reliability: System must maintain operation through power fluctuations and other disruptions
- Usability: Interface must be intuitive for both technicians and researchers

## 📏 Code Style & Conventions
### General
- Max file length: 500 lines (enforce modularization)
- Documentation: All functions and classes must have docstrings
- Comments: Use inline comments explaining "why" not "what"

### Naming Conventions
- Variables: camelCase
- Functions: camelCase
- Classes: PascalCase
- Constants: UPPER_SNAKE_CASE

### Style Guide
- C/C++: Follow Arduino/PlatformIO conventions
- HTML/CSS/JavaScript: Standard web conventions

## 🗺️ Project Roadmap
### Phase 1: Foundation
- Set up project structure
- Implement basic hardware interfaces
- Develop core sensor and control components

### Phase 2: Core Development
- Build control algorithms for all bioreactor parameters
- Integrate network and web interface components
- Implement data logging and visualization

### Phase 3: Testing & Refinement
- Comprehensive testing of all control systems
- Optimize performance and reliability
- User testing and feedback integration

### Phase 4: Launch & Iteration
- Documentation finalization
- Field deployment
- Ongoing maintenance and feature enhancements

## 📚 References
<!-- Include any useful documentation links, articles, or resources -->
- [Project Documentation]
- [Framework Documentation]
- [AI Tools Documentation]