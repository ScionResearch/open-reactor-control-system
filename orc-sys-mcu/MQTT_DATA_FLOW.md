# ORC System MQTT Data Flow & Topic Registry

This document describes how sensor data is published from the Open Reactor Control System (ORC) to an MQTT broker, including topic structure, payload format, and sensor mapping.

---

## Topic Prefix (Device Scoping)

All topics are scoped per device using a prefix:

- **Default:** `orcs/dev/<MAC>` (e.g., `orcs/dev/AA:BB:CC:DD:EE:FF`)
- **Override:** Set `mqttDevicePrefix` via `/api/mqtt` endpoint

All topics below are **relative** to the device prefix.

---

## MQTT Topic Structure

### 1. Input Sensors (from I/O Controller via IPC)

These topics are published by `mqttPublishIPCSensors()` which iterates through the object cache.

| Sensor Type         | Topic Path (relative)      | Example Full Topic                                  | Object Type                    |
|---------------------|---------------------------|-----------------------------------------------------|--------------------------------|
| Temperature         | `sensors/temperature/<id>` | `orcs/dev/AA:BB:CC:DD:EE:FF/sensors/temperature/0`  | `OBJ_T_TEMPERATURE_SENSOR`     |
| pH                  | `sensors/ph/<id>`          | `orcs/dev/AA:BB:CC:DD:EE:FF/sensors/ph/0`           | `OBJ_T_PH_SENSOR`              |
| Dissolved Oxygen    | `sensors/do/<id>`          | `orcs/dev/AA:BB:CC:DD:EE:FF/sensors/do/0`           | `OBJ_T_DISSOLVED_OXYGEN_SENSOR`|
| Optical Density     | `sensors/od/<id>`          | `orcs/dev/AA:BB:CC:DD:EE:FF/sensors/od/0`           | `OBJ_T_OPTICAL_DENSITY_SENSOR` |
| Flow                | `sensors/flow/<id>`        | `orcs/dev/AA:BB:CC:DD:EE:FF/sensors/flow/0`         | `OBJ_T_FLOW_SENSOR`            |
| Pressure            | `sensors/pressure/<id>`    | `orcs/dev/AA:BB:CC:DD:EE:FF/sensors/pressure/0`     | `OBJ_T_PRESSURE_SENSOR`        |
| Power               | `sensors/power/<id>`       | `orcs/dev/AA:BB:CC:DD:EE:FF/sensors/power/0`        | `OBJ_T_POWER_SENSOR`           |
| Energy              | `sensors/energy/<id>`      | `orcs/dev/AA:BB:CC:DD:EE:FF/sensors/energy/0`       | `OBJ_T_ENERGY_SENSOR`          |
| Analog Input        | `sensors/analog/<id>`      | `orcs/dev/AA:BB:CC:DD:EE:FF/sensors/analog/0`       | `OBJ_T_ANALOG_INPUT`           |
| Digital Input       | `sensors/digital/<id>`     | `orcs/dev/AA:BB:CC:DD:EE:FF/sensors/digital/0`      | `OBJ_T_DIGITAL_INPUT`          |

### 2. Actuators (from I/O Controller via IPC)

| Actuator Type       | Topic Path (relative)       | Example Full Topic                                   | Object Type            |
|---------------------|----------------------------|------------------------------------------------------|------------------------|
| Digital Output      | `actuators/digital/<id>`   | `orcs/dev/AA:BB:CC:DD:EE:FF/actuators/digital/0`     | `OBJ_T_DIGITAL_OUTPUT` |
| Analog Output       | `actuators/analog/<id>`    | `orcs/dev/AA:BB:CC:DD:EE:FF/actuators/analog/0`      | `OBJ_T_ANALOG_OUTPUT`  |
| Stepper Motor       | `actuators/stepper/<id>`   | `orcs/dev/AA:BB:CC:DD:EE:FF/actuators/stepper/0`     | `OBJ_T_STEPPER_MOTOR`  |
| DC Motor            | `actuators/dcmotor/<id>`   | `orcs/dev/AA:BB:CC:DD:EE:FF/actuators/dcmotor/0`     | `OBJ_T_BDC_MOTOR`      |

### 3. Controllers (from I/O Controller via IPC)

| Controller Type     | Topic Path (relative)          | Example Full Topic                                       | Object Type                      |
|---------------------|-------------------------------|----------------------------------------------------------|----------------------------------|
| Temperature Control | `controllers/temperature/<id>` | `orcs/dev/AA:BB:CC:DD:EE:FF/controllers/temperature/0`   | `OBJ_T_TEMPERATURE_CONTROL`      |
| pH Control          | `controllers/ph/<id>`          | `orcs/dev/AA:BB:CC:DD:EE:FF/controllers/ph/0`            | `OBJ_T_PH_CONTROL`               |
| Flow Control        | `controllers/flow/<id>`        | `orcs/dev/AA:BB:CC:DD:EE:FF/controllers/flow/0`          | `OBJ_T_FLOW_CONTROL`             |
| DO Control          | `controllers/do/<id>`          | `orcs/dev/AA:BB:CC:DD:EE:FF/controllers/do/0`            | `OBJ_T_DISSOLVED_OXYGEN_CONTROL` |
| OD Control          | `controllers/od/<id>`          | `orcs/dev/AA:BB:CC:DD:EE:FF/controllers/od/0`            | `OBJ_T_OPTICAL_DENSITY_CONTROL`  |
| Gas Flow Control    | `controllers/gasflow/<id>`     | `orcs/dev/AA:BB:CC:DD:EE:FF/controllers/gasflow/0`       | `OBJ_T_GAS_FLOW_CONTROL`         |
| Stirrer Control     | `controllers/stirrer/<id>`     | `orcs/dev/AA:BB:CC:DD:EE:FF/controllers/stirrer/0`       | `OBJ_T_STIRRER_CONTROL`          |
| Pump Control        | `controllers/pump/<id>`        | `orcs/dev/AA:BB:CC:DD:EE:FF/controllers/pump/0`          | `OBJ_T_PUMP_CONTROL`             |
| Device Control      | `controllers/device/<id>`      | `orcs/dev/AA:BB:CC:DD:EE:FF/controllers/device/0`        | `OBJ_T_DEVICE_CONTROL`           |

### 4. External Devices (from I/O Controller via IPC)

| Device Type         | Topic Path (relative)          | Example Full Topic                                       | Object Type              |
|---------------------|-------------------------------|----------------------------------------------------------|--------------------------|
| Hamilton pH Probe   | `devices/hamilton_ph/<id>`     | `orcs/dev/AA:BB:CC:DD:EE:FF/devices/hamilton_ph/0`       | `OBJ_T_HAMILTON_PH_PROBE`|
| Hamilton DO Probe   | `devices/hamilton_do/<id>`     | `orcs/dev/AA:BB:CC:DD:EE:FF/devices/hamilton_do/0`       | `OBJ_T_HAMILTON_DO_PROBE`|
| Hamilton OD Probe   | `devices/hamilton_od/<id>`     | `orcs/dev/AA:BB:CC:DD:EE:FF/devices/hamilton_od/0`       | `OBJ_T_HAMILTON_OD_PROBE`|
| Alicat MFC          | `devices/alicat_mfc/<id>`      | `orcs/dev/AA:BB:CC:DD:EE:FF/devices/alicat_mfc/0`        | `OBJ_T_ALICAT_MFC`       |

### 5. System Status (from RP2040 Controller)

| Data Type           | Topic Path (relative)          | Description                           |
|---------------------|-------------------------------|---------------------------------------|
| PSU Voltage         | `sensors/power/voltage`        | Main PSU voltage (V)                  |
| 20V Rail            | `sensors/power/20v`            | 20V rail voltage (V)                  |
| 5V Rail             | `sensors/power/5v`             | 5V rail voltage (V)                   |
| PSU Status          | `status/psu_ok`                | PSU OK (1=OK, 0=Fault)                |
| 20V Status          | `status/20v_ok`                | 20V rail OK (1=OK, 0=Fault)           |
| 5V Status           | `status/5v_ok`                 | 5V rail OK (1=OK, 0=Fault)            |
| SD Card Status      | `status/sdcard_ok`             | SD card OK (1=OK, 0=Fault)            |
| IPC Status          | `status/ipc_ok`                | IPC OK (1=OK, 0=Fault)                |
| IPC Connected       | `status/ipc_connected`         | IPC connected (1=Yes, 0=No)           |
| IPC Timeout         | `status/ipc_timeout`           | IPC timeout (1=Timeout, 0=OK)         |
| RTC Status          | `status/rtc_ok`                | RTC OK (1=OK, 0=Fault)                |
| Modbus Configured   | `status/modbus_configured`     | Modbus configured (1=Yes, 0=No)       |
| Modbus Connected    | `status/modbus_connected`      | Modbus connected (1=Yes, 0=No)        |
| Modbus Fault        | `status/modbus_fault`          | Modbus fault (1=Fault, 0=OK)          |
| Webserver Status    | `status/webserver_up`          | Webserver up (1=Up, 0=Down)           |
| Webserver Busy      | `status/webserver_busy`        | Webserver busy (1=Busy, 0=Idle)       |
| MQTT Connected      | `status/mqtt_connected`        | MQTT connected (1=Yes, 0=No)          |
| MQTT Busy           | `status/mqtt_busy`             | MQTT busy (1=Busy, 0=Idle)            |

### 6. Consolidated Topics

| Topic Path (relative)  | Description                                          |
|-----------------------|------------------------------------------------------|
| `sensors/all`          | All system status data in one JSON payload           |

---

## Payload Formats

### Standard Sensor Payload

```json
{
  "timestamp": "2025-07-19T10:30:00Z",
  "value": 37.50,
  "unit": "°C",
  "name": "Reactor Temperature",
  "online": true
}
```

### Sensor with Fault

```json
{
  "timestamp": "2025-07-19T10:30:00Z",
  "value": 0.0,
  "unit": "°C",
  "name": "Reactor Temperature",
  "fault": true
}
```

### Controller Payload

Controllers include process value, setpoint, and output:

```json
{
  "timestamp": "2025-07-19T10:30:00Z",
  "processValue": 37.2,
  "setpoint": 37.0,
  "output": 45.5,
  "unit": "°C",
  "name": "Temperature PID"
}
```

### Motor/Actuator Payload

```json
{
  "timestamp": "2025-07-19T10:30:00Z",
  "value": 500.0,
  "unit": "RPM",
  "name": "Stirrer",
  "running": true,
  "direction": "forward"
}
```

### Consolidated System Status (`sensors/all`)

```json
{
  "sensors": {
    "orcs/dev/AA:BB:CC:DD:EE:FF/sensors/power/voltage": {
      "value": 23.62,
      "timestamp": "2025-07-18T14:23:45Z"
    },
    "orcs/dev/AA:BB:CC:DD:EE:FF/sensors/power/20v": {
      "value": 19.93,
      "timestamp": "2025-07-18T14:23:45Z"
    },
    "orcs/dev/AA:BB:CC:DD:EE:FF/status/psu_ok": {
      "value": 1.00,
      "timestamp": "2025-07-18T14:23:45Z"
    }
  }
}
```

---

## Data Flow Summary

### IPC Sensors (from I/O Controller)

The system uses **two publishing mechanisms** for IPC sensor data:

#### 1. Periodic Batch Publishing (`mqttPublishIPCSensors()`)
This is the **primary mechanism** for IPC sensor data:
1. Sensor data arrives from the I/O Controller via IPC
2. Data is cached in the object cache on the RP2040
3. `mqttPublishIPCSensors()` iterates through valid, non-stale objects periodically
4. Each object is mapped to its topic path based on `IPC_ObjectType` enum and published with JSON payload
5. Rate-limited to max 90 objects per cycle to prevent flooding

#### 2. Event-Driven Publishing (`publishSensorData()` / `publishSensorDataIPC()`)
Legacy mechanism for immediate publishing:
- Uses `MqttTopicRegistry.h` to map `MessageTypes` to topic paths
- Called directly from IPC callbacks (currently disabled in favor of batch publishing)

### System Status (RP2040 Local)

1. RP2040 monitors its own systems (power, network, storage, etc.)
2. `mqttPublishAllSensorData()` publishes each status to individual topics
3. Consolidated payload is published to `sensors/all`

---

## Reliability Features

- **KeepAlive:** 30 seconds
- **Socket Timeout:** 10 seconds
- **Buffer Size:** 512 bytes
- **Reconnect Interval:** 5 seconds (backoff)
- **Configurable Publish Interval:** Set via `/api/mqtt` (`mqttPublishIntervalMs`)
- **Rate Limiting:** Max 90 objects published per cycle to prevent flooding

> **Note:** LWT (Last Will and Testament) is declared but not currently implemented. The variable `lwtConfigured` exists but the `reconnect()` function does not pass LWT parameters to `mqttClient.connect()`. This is a TODO for future implementation.

---

## Subscription Examples

```bash
# Subscribe to all data from a specific device
mosquitto_sub -h broker -t "orcs/dev/AA:BB:CC:DD:EE:FF/#"

# Subscribe to all temperature sensors from a device
mosquitto_sub -h broker -t "orcs/dev/AA:BB:CC:DD:EE:FF/sensors/temperature/+"

# Subscribe to all sensors from all devices
mosquitto_sub -h broker -t "orcs/dev/+/sensors/#"

# Subscribe to all system status from a device
mosquitto_sub -h broker -t "orcs/dev/AA:BB:CC:DD:EE:FF/status/#"

# Subscribe to consolidated status from a device
mosquitto_sub -h broker -t "orcs/dev/AA:BB:CC:DD:EE:FF/sensors/all"

# Subscribe to everything from all devices
mosquitto_sub -h broker -t "orcs/dev/#"

# Example with IP address
mosquitto_sub -h 10.10.37.118 -p 1883 -t "orcs/dev/#" -v
```

---

## Notes

- All payloads are JSON with ISO 8601 UTC timestamps
- Topic paths are **relative** - full path is `<devicePrefix>/<relativePath>`
- Object IDs (`<id>`) are 0-indexed integers assigned during I/O configuration
- The registry design allows easy addition of new sensor types
- Both individual topics and consolidated topics are published for flexibility