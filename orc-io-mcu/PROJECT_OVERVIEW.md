# Open Reactor Control System - IO MCU
## Project Overview & Architecture Documentation

**Last Updated:** 2025-10-04 14:06

**Recent Changes:**
- Reorganized drivers into `onboard/` and `peripheral/` subdirectories
- Converted peripheral drivers to class-based architecture for multiple instance support
- Added Hamilton Arc common definitions with unit code lookup table (32 unit types)
- Implemented automatic unit reading for Hamilton Arc sensors
- Added HamiltonArcDO driver for dissolved oxygen sensors
- Added HamiltonArcOD driver for optical density sensors
- Expanded sensor unit fields from 5 to 8 characters to accommodate longer unit names

## 1. PROJECT PURPOSE

This is the firmware for the IO MCU (Input/Output Microcontroller Unit) of an Open Reactor Control System (ORC). It's designed to interface with and manage various sensors, outputs, and communication protocols for a bioreactor or similar laboratory equipment.

## 2. HARDWARE PLATFORM

- **MCU:** ATSAME51N20A (ARM Cortex-M4)
  - Custom board: `scion_orc_m4`
  - 120MHz clock speed
  - Framework: Arduino for SAMD
- **Platform:** atmelsam (PlatformIO)
- **Key External Library:** FlashStorage_SAMD@^1.3.2 for EEPROM emulation

## 3. SYSTEM ARCHITECTURE

### 3.1 Core Design Pattern
The system follows a **driver-based, object-oriented architecture** with:
- Centralized initialization in `sys_init.h/cpp`
- Modular drivers for each hardware peripheral
- Task-based scheduling for periodic operations
- Object index system for dynamic sensor/output management
- Calibration support for analog I/O

### 3.2 File Structure
```
orc-io-mcu/
├── lib/                         # Custom libraries
│   ├── Scheduler/              # Custom task scheduler with CPU monitoring
│   ├── modbus-rtu-master/      # Custom non-blocking Modbus RTU master
│   ├── MCP3464/                # 8-channel ADC driver
│   ├── MCP48FEB/               # 2-channel DAC driver
│   ├── MAX31865/               # RTD temperature sensor driver
│   ├── TMC5130/                # Stepper motor driver
│   ├── DRV8235/                # DC motor driver
│   └── INA260/                 # Power sensor driver
├── src/
│   ├── drivers/                # Hardware abstraction drivers
│   │   ├── objects.h/cpp      # Object type definitions & index
│   │   ├── onboard/           # Fixed onboard device drivers
│   │   │   ├── drv_adc.*          # Analog input driver (8 channels)
│   │   │   ├── drv_dac.*          # Analog output driver (2 channels)
│   │   │   ├── drv_rtd.*          # RTD temperature sensors (3x MAX31865)
│   │   │   ├── drv_gpio.*         # GPIO (8 main + 15 expansion)
│   │   │   ├── drv_output.*       # Digital outputs (4 + 1 heater)
│   │   │   ├── drv_modbus.*       # Modbus base driver (4 ports)
│   │   │   ├── drv_stepper.*      # TMC5130 stepper driver
│   │   │   ├── drv_bdc_motor.*    # DRV8235 motor driver (4x)
│   │   │   └── drv_pwr_sensor.*   # INA260 power sensors (2x)
│   │   └── peripheral/        # External device class-based drivers
│   │       ├── drv_modbus_hamilton_arc_common.h  # Hamilton Arc common definitions
│   │       ├── drv_modbus_hamilton_ph.*          # Hamilton pH probe (class)
│   │       ├── drv_modbus_hamilton_arc_do.*      # Hamilton Arc DO sensor (class)
│   │       ├── drv_modbus_hamilton_arc_od.*      # Hamilton Arc OD sensor (class)
│   │       └── drv_modbus_alicat_mfc.*           # Alicat MFC (class)
│   ├── hardware/
│   │   └── pins.h             # Pin definitions (77 pins defined)
│   ├── tasks/
│   │   └── taskManager.*      # Task instances and scheduler
│   ├── utility/
│   │   └── calibrate.*        # Calibration data management
│   ├── sys_init.h             # System-wide includes
│   └── main.cpp               # Main program
└── platformio.ini             # Build configuration
```

## 4. OBJECT SYSTEM

### 4.1 Object Types (40+ possible objects)
Defined in `objects.h`, the system supports heterogeneous objects via tagged unions:

**Sensors:**
- AnalogInput_t (8x) - Voltage/current inputs
- DigitalIO_t (8 + 15 expansion)
- TemperatureSensor_t (3x RTD)
- PhSensor_t, DissolvedOxygenSensor_t, OpticalDensitySensor_t
- FlowSensor_t, PressureSensor_t
- PowerSensor_t (2x INA260)

**Outputs:**
- AnalogOutput_t (2x DAC)
- DigitalOutput_t (4 + 1 heater with PWM support)

**Actuators:**
- StepperDevice_t (1x TMC5130)
- MotorDevice_t (4x DRV8235)

**Control Objects:**
- TemperatureControl_t, PhControl_t, DissolvedOxygenControl_t
- gasFlowControl_t, stirrerControl_t, pumpControl_t
- feedControl_t, wasteControl_t

### 4.2 Object Index System
- Global array: `objIndex[MAX_NUM_OBJECTS]` (80 max)
- Each entry contains: type tag, void pointer, name string, valid flag
- Allows dynamic runtime enumeration of all system objects
- Used for communication with main control MCU

### 4.3 Calibration System
- Each analog I/O has a `Calibrate_t` object with scale/offset
- 20-entry calibration table stored in emulated EEPROM (FlashStorage_SAMD)
- Two-point calibration support: `calibrate_calc(expected_1, expected_2, actual_1, actual_2)`
- Version control to force re-initialization when structure changes

## 5. TASK SCHEDULING SYSTEM

### 5.1 Custom Scheduler Library
Located in `lib/Scheduler/`, provides:
- **Non-blocking execution** with millisecond precision
- **Priority support** (high-priority tasks run first)
- **Repeat/one-shot modes**
- **CPU usage tracking** per task (10-second rolling window)
- **Performance statistics** (min/max/avg execution time)

### 5.2 Active Tasks (from main.cpp)
```cpp
analog_input_task    → ADC_update()              @ 10ms   (high priority)
output_task          → output_update()           @ 100ms
gpio_task            → gpio_update()             @ 100ms  (high priority)
modbus_task          → modbus_manage()           @ 10ms   (high priority)
phProbe_task         → modbusHamiltonPH_manage() @ 2000ms
mfc_task             → modbusAlicatMFC_manage()  @ 2000ms
RTDsensor_task       → RTD_manage()              @ 200ms
printStuff_task      → printStuff()              @ 1000ms
SchedulerAlive_task  → schedulerHeatbeat()       @ 1000ms
TEST_TASK            → testTaskFunction()        @ 5000ms
```

High-priority tasks are executed first in each scheduler cycle.

## 6. DRIVER DETAILS

### 6.1 Analog Input (ADC) Driver
- **IC:** MCP3464 (24-bit, 8-channel ADC)
- **Communication:** SPI
- **Features:**
  - Continuous scan mode (all 8 channels)
  - Voltage divider ratio: 5.024x
  - Configurable units: mV (default), V, mA, µV
  - Per-channel calibration support
  - 314µV per LSB base resolution
- **Current Mappings (from main.cpp):**
  - Ch 1-2: V mode
  - Ch 3-4: mA mode (240Ω current sensing resistor)
  - Ch 5-6: µV mode
  - Ch 7-8: Default (mV)

### 6.2 Analog Output (DAC) Driver
- **IC:** MCP48FEB (12-bit, 2-channel DAC)
- **Communication:** SPI
- **Features:**
  - 0-10.24V output range (with 5x op-amp gain)
  - 2.048V internal reference
  - 2.50mV per LSB
  - Per-channel calibration
  - Settings saved to IC's EEPROM

### 6.3 RTD Temperature Sensors
- **IC:** MAX31865 (3 instances)
- **Communication:** SPI (individual CS pins)
- **Features:**
  - PT100 or PT1000 sensor support
  - 2-wire, 3-wire, or 4-wire configuration
  - Auto-conversion mode
  - Comprehensive fault detection
  - Per-sensor calibration

### 6.4 Digital Outputs
- **Standard Outputs (4x):** Open-drain, PWM-capable (PIN_OUT_1 to PIN_OUT_4)
  - 8-bit PWM resolution (standard Arduino PWM)
- **Heater Output (1x):** High-current, slow PWM (PIN_HEAT_OUT)
  - Custom TCC0 timer configuration
  - 1Hz PWM frequency for SSR control
  - 117187 period value (120MHz / 1024 prescaler / 1Hz)

### 6.5 GPIO
- **Main GPIO:** 8 pins (PIN_GPIO_0 to PIN_GPIO_7)
- **Expansion GPIO:** 15 pins (PIN_SP_IO_0 to PIN_SP_IO_14)
- **Features:**
  - Configurable input/output per pin
  - Pullup enable/disable
  - Runtime reconfiguration via `configChanged` flag

### 6.6 Modbus RTU Communication

#### 6.6.1 Custom Modbus Library
Located in `lib/modbus-rtu-master/`, key features:
- **Non-blocking operation** with FIFO queue (10 requests max)
- **RS485 support** with DE/RE pin control
- **All standard Modbus functions:** 0x01-0x06, 0x0F, 0x10
- **Callback-based responses** for async operation
- **CRC validation** and timeout handling
- **Critical fixes applied** (from memories):
  - Time unit consistency (millis throughout)
  - Proper timeout calculations
  - Inter-frame delays (3.5 character times)

#### 6.6.2 Modbus Infrastructure
- **4 Modbus ports:**
  - Port 0: Serial2 (RS232)
  - Port 1: Serial3 (RS232)
  - Port 2: Serial4 (RS485 with PIN_RS485_DE_1)
  - Port 3: Serial5 (RS485 with PIN_RS485_DE_2)
- **Runtime configuration** via `configChanged` flag
- **Flexible baud rates and serial config** (stop bits, parity, data bits)

#### 6.6.3 Peripheral Driver Architecture (Class-Based)

**Design Philosophy:**
- **Onboard drivers** (in `drivers/onboard/`): Fixed hardware on PCB, single-instance, struct-based
- **Peripheral drivers** (in `drivers/peripheral/`): External devices, multi-instance support via C++ classes

**Class-Based Peripheral Drivers:**

##### HamiltonPHProbe Class
- **Location:** `drivers/peripheral/drv_modbus_hamilton_ph.*`
- **Interface:** Modbus RTU slave device
- **Register Map:**
  - 2089: pH value (IEEE 754 float, 2 registers)
  - 2409: Temperature (IEEE 754 float, 2 registers)
- **Key Methods:**
  - `HamiltonPHProbe(ModbusDriver_t*, slaveID)` - Constructor
  - `update()` - Queue Modbus requests for pH and temperature
  - `getPhSensor()` - Get PhSensor_t reference
  - `getTemperatureSensor()` - Get TemperatureSensor_t reference
  - `hasFault()` - Check if either sensor has a fault
  - `hasNewMessage()` - Check if there's a new message
  - `getMessage()` - Get message string (prioritizes faults)
  - `clearMessages()` - Clear message flags
- **Features:**
  - Multiple instance support (create as many probes as needed)
  - Non-blocking via callbacks
  - Automatic unit reading from Hamilton unit code
  - Slave ID configurable per instance
  - Unified fault/message interface
  - Uses static callback trampolines for instance routing
- **Typical update rate:** 2000ms

##### HamiltonArcDO Class
- **Location:** `drivers/peripheral/drv_modbus_hamilton_arc_do.*`
- **Interface:** Modbus RTU slave device
- **Register Map:**
  - 2089 (PMC 1): Dissolved Oxygen value with units (IEEE 754 float, 10 registers)
  - 2409 (PMC 6): Temperature with units (IEEE 754 float, 10 registers)
- **Key Methods:**
  - `HamiltonArcDO(ModbusDriver_t*, slaveID)` - Constructor
  - `update()` - Queue Modbus requests for DO and temperature
  - `getDOSensor()` - Get DissolvedOxygenSensor_t reference
  - `getTemperatureSensor()` - Get TemperatureSensor_t reference
  - `hasFault()` - Check if either sensor has a fault
  - `hasNewMessage()` - Check if there's a new message
  - `getMessage()` - Get message string (prioritizes faults)
  - `clearMessages()` - Clear message flags
- **Features:**
  - Multiple instance support (create as many DO sensors as needed)
  - Non-blocking via callbacks
  - Automatic unit reading from Hamilton unit code
  - Slave ID configurable per instance
  - Unified fault/message interface
  - Uses static callback trampolines for instance routing
- **Typical update rate:** 2000ms

##### HamiltonArcOD Class
- **Location:** `drivers/peripheral/drv_modbus_hamilton_arc_od.*`
- **Interface:** Modbus RTU slave device
- **Register Map:**
  - 2089 (PMC 1): Optical Density value with units (IEEE 754 float, 10 registers)
  - 2409 (PMC 6): Temperature with units (IEEE 754 float, 10 registers)
- **Key Methods:**
  - `HamiltonArcOD(ModbusDriver_t*, slaveID)` - Constructor
  - `update()` - Queue Modbus requests for OD and temperature
  - `getODSensor()` - Get OpticalDensitySensor_t reference
  - `getTemperatureSensor()` - Get TemperatureSensor_t reference
  - `hasFault()` - Check if either sensor has a fault
  - `hasNewMessage()` - Check if there's a new message
  - `getMessage()` - Get message string (prioritizes faults)
  - `clearMessages()` - Clear message flags
- **Features:**
  - Multiple instance support (create as many OD sensors as needed)
  - Non-blocking via callbacks
  - Automatic unit reading from Hamilton unit code
  - Slave ID configurable per instance
  - Unified fault/message interface
  - Uses static callback trampolines for instance routing
- **Typical update rate:** 2000ms

##### AlicatMFC Class
- **Location:** `drivers/peripheral/drv_modbus_alicat_mfc.*`
- **Interface:** Modbus RTU slave device
- **Register Map (starting at 1349):**
  - 1349: Setpoint (float)
  - 1351: Valve Drive (float)
  - 1353: Pressure (float)
  - 1355: Secondary Pressure (float)
  - 1357: Barometric Pressure (float)
  - 1359: Temperature (float)
  - 1361: Volumetric Flow (float)
  - 1363: Mass Flow (float)
- **Key Methods:**
  - `AlicatMFC(ModbusDriver_t*, slaveID)` - Constructor
  - `update()` - Queue Modbus request for sensor data
  - `writeSetpoint(float)` - Write new setpoint with automatic validation
  - `getFlowSensor()` - Get FlowSensor_t reference
  - `getPressureSensor()` - Get PressureSensor_t reference
  - `getSetpoint()` - Get current setpoint
  - `hasFault()`, `hasNewMessage()`, `clearMessage()` - Message handling
- **Features:**
  - Multiple instance support (multiple MFCs on different slave IDs)
  - Setpoint write with automatic validation on next read
  - Automatic retry on write failure (up to 5 attempts)
  - Per-instance fault and message tracking
  - Uses static callback trampolines for instance routing
- **Typical update rate:** 2000ms

**Unified API Across All Peripheral Drivers:**

All peripheral device drivers share a consistent interface:
- `update()` - Non-blocking update method
- `getSlaveID()` - Get Modbus slave ID
- `hasFault()` - Check for fault condition
- `hasNewMessage()` - Check for new messages
- `getMessage()` - Get message string (prioritizes faults)
- `clearMessages()` or `clearMessage()` - Clear message flags

**Usage Example:**
```cpp
// Create instances (typically in main.cpp)
HamiltonPHProbe* phProbe1 = new HamiltonPHProbe(&modbusDriver[2], 3);
HamiltonPHProbe* phProbe2 = new HamiltonPHProbe(&modbusDriver[2], 4);
HamiltonArcDO* doSensor = new HamiltonArcDO(&modbusDriver[2], 5);
HamiltonArcOD* odSensor = new HamiltonArcOD(&modbusDriver[2], 6);
AlicatMFC* mfc1 = new AlicatMFC(&modbusDriver[3], 1);
AlicatMFC* mfc2 = new AlicatMFC(&modbusDriver[3], 2);

// Add to scheduler using lambda functions
tasks.addTask([]() { phProbe1->update(); }, 2000, true, false);
tasks.addTask([]() { doSensor->update(); }, 2000, true, false);
tasks.addTask([]() { odSensor->update(); }, 2000, true, false);
tasks.addTask([]() { mfc1->update(); }, 2000, true, false);

// Access sensor data
float pH = phProbe1->getPhSensor().ph;
const char* phUnit = phProbe1->getPhSensor().unit;
float dissolvedOxygen = doSensor->getDOSensor().dissolvedOxygen;
float opticalDensity = odSensor->getODSensor().opticalDensity;
float flow = mfc1->getFlowSensor().flow;

// Write setpoint to MFC
mfc1->writeSetpoint(50.0);

// Consistent fault/message handling across all devices
if (phProbe1->hasFault()) {
    Serial.printf("pH Probe Error: %s\n", phProbe1->getMessage());
    phProbe1->clearMessages();
}
if (mfc1->hasFault()) {
    Serial.printf("MFC Error: %s\n", mfc1->getMessage());
    mfc1->clearMessage();
}
```

### 6.7 Stepper Motor (TMC5130)
- **Communication:** SPI
- **Features:** StealthChop, configurable current, RPM control
- **Current state:** Commented out in main.cpp (not actively used)

### 6.8 DC Motors (DRV8235)
- **Count:** 4 motors
- **Features:** Bidirectional control, current feedback
- **Current state:** Partially implemented

### 6.9 Power Sensors (INA260)
- **Count:** 2 sensors
- **Communication:** I2C
- **Measures:** Voltage, current, power
- **Current state:** Driver present but not actively used

## 7. COMMUNICATION PORTS

### 7.1 Serial Ports
- **Serial (USB):** 115200 baud - Debug/monitoring
- **Serial2:** RS232 Port 1
- **Serial3:** RS232 Port 2
- **Serial4:** RS485 Port 1 (with DE/RE control)
- **Serial5:** RS485 Port 2 (with DE/RE control)

### 7.2 SPI Devices
All SPI devices share the main SPI bus but have individual CS pins:
- ADC (MCP3464): PIN_ADC_CS
- DAC (MCP48FEB): PIN_DAC_CS
- RTD 1-3 (MAX31865): PIN_PT100_CS_1/2/3
- Stepper (TMC5130): PIN_STP_CS

### 7.3 I2C Devices
- I2C bus: PIN_I2C_PER_SDA, PIN_I2C_PER_SCL
- Power sensors (INA260) on I2C

## 8. INITIALIZATION SEQUENCE

From `main.cpp setup()`:
1. Serial port (115200 baud)
2. CS pins initialization
3. Calibration data load from EEPROM
4. ADC initialization (continuous scan mode)
5. RTD sensors (3x, configured as needed)
6. DAC initialization (save to IC EEPROM)
7. ADC input unit configuration
8. Outputs initialization (standard + heater PWM)
9. GPIO initialization
10. Modbus initialization (4 ports)
11. Modbus port 2 reconfiguration (19200 baud, 2 stop bits for pH)
12. Modbus port 3 reconfiguration (19200 baud for MFC)
13. Hamilton pH probe driver initialization
14. Alicat MFC driver initialization
15. Task creation and registration
16. Enter main loop (tasks.update())

## 9. KEY DESIGN PATTERNS

### 9.1 Onboard Driver Structure Pattern
Onboard (fixed PCB hardware) drivers follow a struct-based pattern:
```cpp
// Object definitions
struct SensorType_t { ... };
SensorType_t sensorArray[N];

// Driver structure
struct DriverType_t {
    SensorType_t *objPointers[N];
    HardwareInterface *hardware;
    bool fault, newMessage;
    char message[100];
};
DriverType_t driver;

// Functions
bool init(void);      // Initialize hardware and objects
void update(void);    // Periodic update function
```

### 9.2 Peripheral Driver Class Pattern
Peripheral (external) drivers use C++ classes for multi-instance support:
```cpp
class PeripheralDevice {
public:
    // Constructor with modbus driver and slave ID
    PeripheralDevice(ModbusDriver_t *modbusDriver, uint8_t slaveID);
    
    // Public methods
    void update();                          // Non-blocking update
    SensorType_t& getSensorData();          // Access sensor objects
    
private:
    ModbusDriver_t *_modbusDriver;          // Pointer to modbus port
    uint8_t _slaveID;                       // Device slave ID
    SensorType_t _sensorData;               // Sensor data storage
    uint16_t _dataBuffer[SIZE];             // Modbus data buffer
    
    // Static callback trampoline pattern
    static PeripheralDevice* _currentInstance;
    static void responseCallback(bool valid, uint16_t *data);
    void handleResponse(bool valid, uint16_t *data);
};
```

**Key aspects of peripheral class pattern:**
- **Multiple instances:** Each device is an independent object
- **Static callback trampolines:** Required because Modbus callbacks must be static functions
- **Instance tracking:** `_currentInstance` static pointer updated before each request
- **Encapsulation:** All device-specific logic contained in class
- **Standard sensor objects:** Uses same `SensorType_t` structs as onboard drivers

### 9.3 Forward Declarations
Used to avoid circular includes (e.g., in `drv_modbus_hamilton_ph.h`):
```cpp
struct ModbusDriver_t;  // Forward declaration
class HamiltonPHProbe {
    ModbusDriver_t *_modbusDriver;  // Pointer to avoid include
    ...
};
```

### 9.4 Struct Member Access
**Critical pattern** (from memory):
- `modbusDriver` is a **pointer** to `ModbusDriver_t`
- `modbus` is a **member** (not pointer) of `ModbusDriver_t`
- Correct syntax: `modbusDriver->modbus.method()`

## 10. CPU USAGE MONITORING

The custom Scheduler library tracks CPU usage:
- **Per-task monitoring** via `getCpuUsagePercent()`
- **System-wide total** via `getTotalCpuUsagePercent()`
- **10-second rolling window**
- **Microsecond precision** execution time tracking
- Statistics: last, min, max, average execution time

Current implementation in main.cpp prints comprehensive CPU reports every 1000ms.

## 11. KNOWN ISSUES & NOTES

### 11.1 From Memories
1. **Modbus timing issues fixed:**
   - Time unit consistency
   - Proper inter-frame delays
   - Correct timeout calculations

2. **Pointer syntax in Modbus drivers:**
   - Correct: `modbusDriver->modbus.method()`
   - Wrong: `modbusDriver.modbus->method()`

### 11.2 From Code Review
1. **Main.cpp is debug-heavy** - lots of testing code
2. **Some drivers disabled** - Stepper, DC motors, power sensors commented out
3. **Float printf support** - Uses `asm(".global _printf_float")` for float formatting
4. **Memory allocation** - Some dynamic allocation in Modbus single-write functions (noted in README)

## 12. FUTURE EXPANSION

The system is designed for expansion:
- Object index supports up to 80 objects
- 20 calibration slots (currently using ~15)
- Unused object types defined (DO, OD, optical density control, etc.)
- Template for adding new Modbus devices
- Reserved spare GPIO pins (15)

## 13. CRITICAL CONSTANTS

### 13.1 ADC Constants
```cpp
#define ADC_V_DIV_RATIO     5.024
#define ADC_uV_PER_LSB      314.0
#define ADC_mV_PER_LSB      0.314
#define ADC_V_PER_LSB       0.000314
#define ADC_mA_PER_LSB      0.001308333  // 240Ω resistor
```

### 13.2 DAC Constants
```cpp
#define DAC_RANGE         4095
#define VREF_mV           2048
#define OP_AMP_GAIN       5
#define VOUT_MAX_mV       10240
#define mV_PER_LSB        2.50
```

### 13.3 Heater PWM
```cpp
#define HEATER_PWM_PERIOD 117187  // 120MHz / 1024 / 1Hz - 1
```

### 13.4 Modbus Timing
```cpp
#define MODBUS_DEFAULT_TIMEOUT 1000  // ms
#define MODBUS_DEFAULT_INTERFRAME_DELAY 3500  // µs
```

## 14. DEPENDENCIES

- **Arduino Framework** for SAMD
- **SPI** (hardware peripheral)
- **Wire** (I2C)
- **FlashStorage_SAMD** (EEPROM emulation)
- **Standard C++ library** (vector, algorithm for Scheduler)

---

**Note:** This document reflects the project state as of 2025-10-04. Refer to git history for changes.
