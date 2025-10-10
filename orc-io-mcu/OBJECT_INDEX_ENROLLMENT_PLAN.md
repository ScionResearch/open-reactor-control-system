# Object Index Enrollment Plan - SAME51 IO MCU

**Date:** 2025-10-11  
**Status:** 🚧 Ready for Implementation  
**Target:** Enroll all 41 onboard objects (indices 0-40) into IPC object index

---

## 1. UPDATED INDEX MAP

Following `IPC_PROTOCOL_SPECIFICATION.md` Section 5:

```
Fixed Indices (0-40): Onboard Hardware
═══════════════════════════════════════
 0-7   : Analog Inputs (ADC)              ✅ DONE (fixed indices)
 8-9   : Analog Outputs (DAC)             ✅ DONE (fixed indices)
 10-12 : RTD Temperature Sensors          ✅ DONE (fixed indices)
 13-20 : Digital GPIO (8 channels)        ✅ DONE (fixed indices)
 21-25 : Digital Outputs (4 + heater)     ✅ DONE (fixed indices)
 26    : Stepper Motor                    ✅ DONE (with fault propagation)
 27-30 : DC Motors (4x)                   ✅ DONE (with fault propagation)
 31-36 : Power Sensors (2×3 values)       ✅ DONE (V/A/W split)
 37-40 : Modbus Ports (4x)                ✅ DONE (SerialCom_t)

Control Objects (41-59): Reserved for future
Dynamic Indices (60-79): User-created devices
```

---

## 2. ESTABLISHED ENROLLMENT PATTERN ✅

**Pattern for Fixed Indices (0-40):**

```cpp
// In driver init function, after creating object:

// Add to object index (fixed index FIXED_INDEX)
objIndex[FIXED_INDEX + i].type = OBJ_T_XXX;
objIndex[FIXED_INDEX + i].obj = &objectArray[i];
sprintf(objIndex[FIXED_INDEX + i].name, "Object Name %d", i + 1);
objIndex[FIXED_INDEX + i].valid = true;

// DO NOT increment numObjects
// DO NOT use objIndex[numObjects] for fixed indices
```

**Key Rules:**
- ✅ Use explicit fixed index: `objIndex[8 + i]` not `objIndex[numObjects]`
- ✅ Never increment `numObjects` for fixed indices
- ✅ Use `sprintf()` instead of `strcpy()` + `strcat()` for names
- ✅ Set `numObjects = 41` once after all fixed objects enrolled

**Examples Implemented:**
- **ADC** (indices 0-7): `objIndex[0 + i]`
- **DAC** (indices 8-9): `objIndex[8 + i]`
- **RTD** (indices 10-12): `objIndex[10 + i]`
- **Modbus** (indices 37-40): `objIndex[37 + i]`

---

## 3. OBJECT STRUCTURE UPDATES

### Add missing fault/message fields to:

**`objects.h` changes:**

```cpp
// DigitalIO_t - ADD:
bool fault;
bool newMessage;
char message[100];

// StepperDevice_t - ADD:
bool fault;
bool newMessage;
char message[100];
char unit[8];  // "rpm"

// MotorDevice_t - ADD:
bool fault;
bool newMessage;
char message[100];
char unit[8];  // "%"

// NEW STRUCT: ModbusPort_t
struct ModbusPort_t {
    uint8_t portNumber;
    uint32_t baudRate;
    uint8_t dataBits;
    uint8_t stopBits;
    uint8_t parity;
    bool enabled;
    uint8_t slaveCount;
    bool fault;
    bool newMessage;
    char message[100];
};

// NEW STRUCT: PowerSensorValue_t (wrapper for V/A/W split)
struct PowerSensorValue_t {
    PowerSensor_t *source;
    uint8_t valueType;  // 0=V, 1=A, 2=W
    bool fault;
    bool newMessage;
    char message[100];
};
```

---

## 3. ENROLLMENT IMPLEMENTATION

### Key Pattern for Fixed Indices:
```cpp
// DO NOT increment numObjects for fixed indices
objIndex[FIXED_INDEX].type = OBJ_T_XXX;
objIndex[FIXED_INDEX].obj = &objectPtr;
strcpy(objIndex[FIXED_INDEX].name, "Object Name");
objIndex[FIXED_INDEX].valid = true;
```

### After all fixed objects enrolled:
```cpp
// In sys_init.cpp after all drivers initialized:
numObjects = 41;  // Set to first dynamic index
```

---

## 4. DRIVER MODIFICATIONS

### 4.1 DAC (`drv_dac.cpp` line ~15)
- Enroll `dacOutput[0]` → index 8
- Enroll `dacOutput[1]` → index 9

### 4.2 RTD (`drv_rtd.cpp` line ~36)
- Enroll `rtd_sensor[0]` → index 10
- Enroll `rtd_sensor[1]` → index 11
- Enroll `rtd_sensor[2]` → index 12

### 4.3 GPIO (`drv_gpio.cpp` line ~40)
- Update `DigitalIO_t` struct first
- Enroll `gpio[0-7]` → indices 13-20
- Skip `gpioExp[15]` (not indexed)

### 4.4 Outputs (`drv_output.cpp` line ~20)
- Enroll `digitalOutput[0-3]` → indices 21-24
- Enroll `heaterOutput[0]` → index 25

### 4.5 Stepper (`drv_stepper.cpp` line ~10)
- Update `StepperDevice_t` struct first
- Enroll `stepperDevice` → index 26
- Propagate faults from driver to device

### 4.6 Motors (`drv_bdc_mot.cpp` line ~15)
- Update `MotorDevice_t` struct first
- Enroll `motorDevice[0-3]` → indices 27-30
- Propagate faults in `motor_update()`

### 4.7 Power Sensors (`drv_pwr_sensor.cpp` line ~10)
- Add `PowerSensorValue_t` wrappers
- Create arrays: `pwr_voltage[2]`, `pwr_current[2]`, `pwr_power[2]`
- Enroll sensor 1: V→31, A→32, W→33
- Enroll sensor 2: V→34, A→35, W→36
- Update IPC handler to extract value based on `valueType`

### 4.8 Modbus (`drv_modbus.cpp` line ~10)
- Add `ModbusPort_t modbusPort[4]` array
- Enroll ports 1-4 → indices 37-40
- Types: 0-1 = RS232, 2-3 = RS485
- Keep port objects in sync with driver config

---

## 5. IPC HANDLER UPDATES

### `ipc_handlers.cpp` - Update `ipc_sendSensorData()`:

Add handling for new object types:
- `OBJ_T_DIGITAL_INPUT` - return `state` as float
- `OBJ_T_DIGITAL_OUTPUT` - return `state` or `pwmDuty`
- `OBJ_T_STEPPER_MOTOR` - return `rpm`
- `OBJ_T_BDC_MOTOR` - return `power`
- `OBJ_T_POWER_SENSOR` - extract V/A/W from wrapper
- `OBJ_T_SERIAL_RS232_PORT` / `OBJ_T_SERIAL_RS485_PORT` - return status

---

## 6. IMPLEMENTATION ORDER

1. **Update `objects.h`** - Add missing struct fields ⚙️
2. **DAC enrollment** - Simple, 2 objects ✅
3. **RTD enrollment** - Simple, 3 objects ✅
4. **GPIO enrollment** - Requires struct update ⚙️✅
5. **Outputs enrollment** - Simple, 5 objects ✅
6. **Stepper enrollment** - Requires struct update ⚙️✅
7. **Motors enrollment** - Requires struct update + fault propagation ⚙️✅
8. **Power sensors** - Complex, needs wrappers ⚙️⚙️✅
9. **Modbus ports** - New struct + enrollment ⚙️⚙️✅
10. **Set `numObjects = 41`** in `sys_init.cpp` ✅
11. **Update IPC handlers** - Add new object type handling ⚙️⚙️✅
12. **Test & verify** - Full system test ✅

---

## 7. TESTING CHECKLIST

After implementation:

- [ ] Compile SAME51 firmware
- [ ] Verify `numObjects == 41` on boot
- [ ] Test HELLO handshake reports 41 objects
- [ ] Test INDEX_SYNC returns all 41 entries
- [ ] Test SENSOR_READ_REQ for each object type
- [ ] Verify power sensor V/A/W split works
- [ ] Verify Modbus port info accessible
- [ ] Check fault propagation on motors/stepper

---

## 8. FILES TO MODIFY

**Core:**
- `src/drivers/objects.h` - Structure updates
- `src/sys_init.cpp` - Set numObjects = 41

**Drivers:**
- `src/drivers/onboard/drv_dac.cpp`
- `src/drivers/onboard/drv_rtd.cpp`
- `src/drivers/onboard/drv_gpio.cpp`
- `src/drivers/onboard/drv_output.cpp`
- `src/drivers/onboard/drv_stepper.cpp`
- `src/drivers/onboard/drv_bdc_mot.cpp`
- `src/drivers/onboard/drv_pwr_sensor.h/cpp`
- `src/drivers/onboard/drv_modbus.h/cpp`

**IPC:**
- `src/drivers/ipc/ipc_handlers.cpp` - Sensor data extraction

---

**Ready to implement?** Start with Step 1 (structure updates) then proceed driver-by-driver. Each driver is independent after structures are updated.
