# IPC Object Index Testing Guide

## Overview
Debug code has been added to both MCUs to verify that sensor data is flowing correctly through the IPC system.

---

## What Was Added

### IO MCU (SAME51N20A) - `orc-io-mcu`
✅ All 41 fixed objects enrolled (indices 0-40)
✅ Fixed index pattern implemented (no more `numObjects++` issues)
✅ Fault propagation added to all drivers
✅ Debug output on startup showing object index map

**On Startup, the IO MCU will print:**
```
=== Object Index Status ===
Fixed objects enrolled: 0-40 (41 objects)
Index map:
  0-7   : Analog Inputs (ADC)
  8-9   : Analog Outputs (DAC)
  10-12 : RTD Temperature Sensors
  13-20 : Digital GPIO
  21-25 : Digital Outputs (4 + heater)
  26    : Stepper Motor
  27-30 : DC Motors
  31-36 : Power Sensors (V/A/W)
  37-40 : Modbus Ports
  41-59 : Reserved for control objects
  60-79 : Dynamic user-created devices
===========================
```

### System MCU (RP2040) - `orc-sys-mcu`
✅ Debug test function added to `ipcManager.cpp`
✅ Automatic sensor read requests every 2 seconds
✅ Cycles through different sensor types to test all categories
✅ Prints received sensor data with values and status

**File:** `orc-sys-mcu/src/utils/ipcManager.cpp`
- Function: `testSensorReads()` (called from `manageIPC()`)
- Can be disabled by setting `testActive = false` in the function

---

## Expected Output

### System MCU Serial Output
Every 2 seconds, you should see:

```
IPC Test: Requesting ADC 0 (index 0)...
  ✓ Sensor[0] Type=0: value=1234.567 mV

IPC Test: Requesting ADC 1 (index 1)...
  ✓ Sensor[1] Type=0: value=2345.678 mV

IPC Test: Requesting DAC 0 (index 8)...
  ✓ Sensor[8] Type=9: value=0.000 mV

IPC Test: Requesting RTD 0 (index 10)...
  ✓ Sensor[10] Type=2: value=23.45 °C

IPC Test: Requesting GPIO 0 (index 13)...
  ✓ Sensor[13] Type=1: value=0.000 

... etc ...

=== IPC Sensor Test Cycle Complete ===
```

### With Faults/Messages
If a sensor has a fault or message:
```
IPC Test: Requesting Stepper (index 26)...
  ✓ Sensor[26] Type=12: value=0.000 rpm [FAULT] MSG: Stepper initialisation failed
```

---

## Test Sequence

The debug code cycles through these sensors every 2 seconds:

| Index | Sensor Type          | Object Type Enum      |
|-------|----------------------|----------------------|
| 0     | ADC 0               | OBJ_T_ANALOG_INPUT   |
| 1     | ADC 1               | OBJ_T_ANALOG_INPUT   |
| 8     | DAC 0               | OBJ_T_ANALOG_OUTPUT  |
| 10    | RTD 0               | OBJ_T_TEMPERATURE_SENSOR |
| 13    | GPIO 0              | OBJ_T_DIGITAL_INPUT  |
| 21    | Output 0            | OBJ_T_DIGITAL_OUTPUT |
| 26    | Stepper             | OBJ_T_STEPPER_MOTOR  |
| 27    | Motor 0             | OBJ_T_BDC_MOTOR      |
| 31    | Voltage 0           | OBJ_T_VOLTAGE_SENSOR |
| 32    | Current 0           | OBJ_T_CURRENT_SENSOR |
| 33    | Power 0             | OBJ_T_POWER_SENSOR   |
| 37    | Modbus 0            | OBJ_T_SERIAL_RS232_PORT |

After completing the cycle (12 sensors × 2s = 24 seconds), it resets and starts over.

---

## How to Test

### 1. **Upload Firmware**
```bash
# IO MCU (SAME51)
cd orc-io-mcu
pio run -e scion_orc_m4 -t upload

# System MCU (RP2040)
cd orc-sys-mcu
pio run -e pico -t upload
```

### 2. **Monitor System MCU Serial Output**
```bash
cd orc-sys-mcu
pio device monitor -e pico
```

### 3. **What to Look For**

✅ **SUCCESS Indicators:**
- IPC handshake completes (HELLO/HELLO_ACK exchange)
- Sensor read requests are sent every 2 seconds
- Sensor data responses are received with valid values
- Object types match expected values
- Units are correct (mV, °C, rpm, V, A, W, etc.)

❌ **FAILURE Indicators:**
- No sensor data responses received
- CRC errors in IPC packets
- Timeout messages
- Incorrect object types
- Missing or corrupted units/values

---

## Disabling Debug Output

To turn off the automatic sensor testing (once you've verified everything works):

**Edit:** `orc-sys-mcu/src/utils/ipcManager.cpp`

**Change line 30 from:**
```cpp
static bool testActive = true;  // Set to false to disable debug output
```

**To:**
```cpp
static bool testActive = false;  // Debug test disabled
```

Then recompile and upload.

---

## Troubleshooting

### No IPC Communication
1. Check physical connections between SAME51 and RP2040
2. Verify baud rate match (2 Mbps on both sides)
3. Check for CRC errors in serial output

### Sensor Data Not Updating
1. Verify drivers are initialized on IO MCU
2. Check IO MCU serial output for initialization errors
3. Ensure tasks are running (check scheduler output)

### Incorrect Values
1. Check calibration data loaded correctly
2. Verify sensor wiring/connections
3. Look for fault flags in sensor data

### Faults Reported
1. Check IO MCU serial output for detailed fault messages
2. Verify hardware connections
3. Check power supply voltages

---

## Next Steps

Once sensor data is flowing correctly:

1. **IPC Handler Expansion** - Add handlers for new sensor types
2. **Control Loop Implementation** - Use indices 41-59 for PID controllers
3. **Dynamic Device Creation** - Use indices 60-79 for user peripherals
4. **MQTT Integration** - Verify sensor data publishes to MQTT broker
5. **Web Dashboard** - Display real-time sensor values

---

## Build Status

### IO MCU (SAME51N20A)
- ✅ **Compiled Successfully**
- RAM: 13.0% (34,008 / 262,144 bytes)
- Flash: 30.9% (324,228 / 1,048,576 bytes)

### System MCU (RP2040)
- ✅ **Compiled Successfully**
- RAM: 32.1% (84,136 / 262,144 bytes)
- Flash: 1.5% (255,404 / 16,510,976 bytes)

---

**Ready to test!** 🚀
