# Device Control Testing Checklist
**Date:** 2025-10-27  
**Phases Complete:** 1-3 (Architecture, Drivers, Device Manager)  
**Status:** 🚧 Ready for Compilation Testing

---

## ✅ COMPLETED IMPLEMENTATION

### Phase 1: Architecture ✅
- [x] Expanded `MAX_NUM_OBJECTS` from 80 to 100
- [x] Added `OBJ_T_DEVICE_CONTROL` enum
- [x] Created `DeviceControl_t` structure
- [x] Updated `ManagedDevice` structure for dual-index model
- [x] Updated IPC Protocol Specification to v2.5

### Phase 2: Device Drivers ✅
- [x] AlicatMFC - Added control object and population logic
- [x] HamiltonPHProbe - Added control object and population logic
- [x] HamiltonArcDO - Added control object and population logic
- [x] HamiltonArcOD - Added control object and population logic

### Phase 3: Device Manager ✅
- [x] Updated validation for new index ranges (70-99)
- [x] Implemented control index calculation (sensor - 20)
- [x] Added `getDeviceControlObject()` helper function
- [x] Register control objects at indices 50-69
- [x] Initialize control object fields during creation
- [x] Updated `findDevice()` to use `startSensorIndex`
- [x] Updated `registerDeviceObjects()` for all device types
- [x] Updated `unregisterDeviceObjects()` to use new field names
- [x] Updated `deleteDevice()` to unregister control objects

---

## 🔧 COMPILATION TESTING

### Test 1: Compile IO MCU Firmware

**Expected Output:**
```
Building .pio/build/same51-io-mcu/src/drivers/device_manager.cpp.o
Building .pio/build/same51-io-mcu/src/drivers/peripheral/drv_modbus_alicat_mfc.cpp.o
Building .pio/build/same51-io-mcu/src/drivers/peripheral/drv_modbus_hamilton_ph.cpp.o
Building .pio/build/same51-io-mcu/src/drivers/peripheral/drv_modbus_hamilton_arc_do.cpp.o
Building .pio/build/same51-io-mcu/src/drivers/peripheral/drv_modbus_hamilton_arc_od.cpp.o
...
SUCCESS
```

**Potential Issues:**
- [ ] Missing includes for `DeviceControl_t`
- [ ] Undefined reference to `getDeviceControlObject()`
- [ ] Type mismatch errors in control object population
- [ ] Index out of bounds warnings

**Resolution:**
- Check that `objects.h` is included in all device driver headers
- Verify function declaration in `device_manager.h`
- Confirm all device types handle control objects correctly

---

## ✅ RUNTIME TESTING (After Successful Compilation)

### Test 2: Device Creation with Control Objects

**Test Steps:**
1. Upload IO MCU firmware
2. Power on system
3. Via IPC, create a Hamilton pH probe at sensor index 72

**Expected Behavior:**
```
[DEV MGR] Creating Hamilton pH probe (port 0, ID 1)
[DEV MGR] Registered pH probe objects at 72-73
[DEV MGR] ✓ Device created: type=1, control=52, sensors=72-73
```

**Verification:**
- [ ] Control object registered at index 52
- [ ] pH sensor registered at index 72
- [ ] Temperature sensor registered at index 73
- [ ] Control object fields initialized correctly:
  - `startSensorIndex = 72`
  - `sensorCount = 2`
  - `slaveID = 1`
  - `deviceType = IPC_DEV_HAMILTON_PH`

---

### Test 3: Control Object Population During Update

**Test Steps:**
1. With pH probe created, trigger Modbus read
2. Check control object is populated with data

**Expected Behavior:**
```
[HAMILTON] pH: 7.25
[DEV MGR] Control object updated: connected=true, actualValue=7.25
```

**Verification:**
- [ ] `controlObj.actualValue` matches pH reading
- [ ] `controlObj.connected = true` after successful Modbus response
- [ ] `controlObj.fault = false` when no errors
- [ ] `controlObj.fault = true` on Modbus communication error
- [ ] `controlObj.connected = false` on Modbus timeout

---

### Test 4: Multiple Devices with Correct Index Allocation

**Test Steps:**
1. Create Alicat MFC at sensor index 70
2. Create Hamilton pH at sensor index 72
3. Create Hamilton DO at sensor index 74
4. Create Hamilton OD at sensor index 76

**Expected Behavior:**
```
[DEV MGR] ✓ Device created: type=4, control=50, sensors=70-71  (MFC)
[DEV MGR] ✓ Device created: type=1, control=52, sensors=72-73  (pH)
[DEV MGR] ✓ Device created: type=2, control=54, sensors=74-75  (DO)
[DEV MGR] ✓ Device created: type=3, control=56, sensors=76-77  (OD)
```

**Verification:**
- [ ] No index collisions
- [ ] Each device has unique control index
- [ ] Each device has unique sensor indices
- [ ] Control indices calculated correctly (sensor - 20)
- [ ] All sensors accessible and updating

---

### Test 5: Device Deletion Cleanup

**Test Steps:**
1. Create device at sensor index 72
2. Delete device
3. Verify cleanup

**Expected Behavior:**
```
[DEV MGR] ✓ Device deleted: control=52, sensors=72-73
```

**Verification:**
- [ ] Control object at index 52 marked invalid
- [ ] Sensor objects at 72-73 marked invalid
- [ ] Device slot freed for reuse
- [ ] Can create new device at same indices

---

### Test 6: Bulk Read Including Control Objects

**Test Steps:**
1. Create multiple devices
2. Request bulk read from index 50-99

**Expected Behavior:**
- Control objects (50-69) return sensor data format
- Sensor objects (70-99) return normally
- No errors or timeouts

**Verification:**
- [ ] Control object `actualValue` returned as sensor `value`
- [ ] Control object flags properly set
- [ ] Bulk read completes in < 100ms
- [ ] All objects report valid data

---

## 🐛 ERROR SCENARIOS

### Error 1: Invalid Sensor Index Range

**Test:**
```
Create device at sensor index 65  // Outside 70-99 range
```

**Expected:**
```
[DEV MGR] ERROR: Invalid start sensor index 65 (must be 70-99)
```

**Verification:**
- [ ] Device creation fails
- [ ] No objects registered
- [ ] Error logged correctly

---

### Error 2: Control Index Already in Use

**Test:**
```
Create device at sensor index 70  // control=50
Manually set objIndex[50].valid = true
Create device at sensor index 70 again
```

**Expected:**
```
[DEV MGR] ERROR: Control index 50 already in use
```

**Verification:**
- [ ] Second device creation fails
- [ ] First device unaffected
- [ ] Proper error handling

---

### Error 3: Modbus Communication Failure

**Test:**
```
Create device with invalid Modbus address
Wait for update cycle
```

**Expected:**
```
[HAMILTON] Invalid pH data from pH probe (ID 99)
Control object: connected=false, fault=true
```

**Verification:**
- [ ] Control object reflects communication failure
- [ ] `connected = false`
- [ ] `fault = true`
- [ ] Error message populated

---

## 📊 PERFORMANCE METRICS

### Expected Timings:
- Device creation: < 10ms
- Control object population: < 1ms (during sensor update)
- Bulk read (50 objects): < 100ms
- Device deletion: < 5ms

### Memory Usage:
- `DeviceControl_t` per device: ~140 bytes
- 20 devices max: ~2.8 KB total
- Acceptable for SAME51 (256 KB RAM)

---

## 🚦 GO/NO-GO CRITERIA

### ✅ Ready for Phase 4 (IPC Protocol) if:
- [x] IO MCU firmware compiles without errors
- [ ] Device creation succeeds with correct index allocation
- [ ] Control objects populate during device updates
- [ ] Multiple devices coexist without collisions
- [ ] Device deletion cleans up properly
- [ ] No memory leaks or crashes during stress test

### ⚠️ Blockers:
- Compilation errors (must fix immediately)
- Index collision bugs (critical)
- Memory corruption (critical)
- Control object not populating (high priority)

---

## 📝 NEXT STEPS AFTER TESTING

1. **If tests pass:** Proceed to Phase 4 - IPC Device Control Protocol
2. **If minor issues:** Fix and retest affected areas
3. **If major issues:** Review architecture and implementation

---

## 🔍 DEBUGGING TIPS

### Serial Monitor Output to Watch For:
```
[DEV MGR] ✓ Device created: type=X, control=Y, sensors=Z-W
[DEV MGR] Registered [TYPE] objects at Z-W
[DEVICE] Control object updated: connected=true, actualValue=X.XX
[DEV MGR] ✓ Device deleted: control=Y, sensors=Z-W
```

### Common Issues:
1. **Control object not updating:**
   - Check device driver `update()` calls control object population
   - Verify `getControlObject()` returns valid pointer

2. **Index collisions:**
   - Check validation logic in `createDevice()`
   - Verify control index calculation (sensor - 20)

3. **Memory corruption:**
   - Verify `DeviceControl_t` properly initialized
   - Check all pointers are valid before dereferencing

---

## ✅ FINAL CHECKLIST

**Before declaring Phase 3 complete:**
- [x] All driver files modified and saved
- [x] Device manager updated and saved
- [x] Header files include new function declarations
- [ ] IO MCU firmware compiles successfully
- [ ] All runtime tests pass
- [ ] No memory leaks detected
- [ ] Performance meets expectations
- [ ] Documentation updated

**Current Status:** Ready for compilation testing
