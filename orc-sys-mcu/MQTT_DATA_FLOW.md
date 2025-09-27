# ORC System MQTT Data Flow & Topic Registry

This document describes how sensor data is published from the Open Reactor Control System (ORC) to an MQTT broker, including topic structure, payload format, and sensor mapping.

---

## MQTT Topic Structure

The ORC system publishes two types of sensor data:

### 1. External Sensor Data (from I/O Controller via IPC)
| Sensor Type         | Topic Prefix                | Example Topic                  |
|---------------------|----------------------------|-------------------------------|
| Power               | orcs/sensors/power         | orcs/sensors/power/0          |
| Temperature         | orcs/sensors/temperature   | orcs/sensors/temperature/0    |
| pH                  | orcs/sensors/ph            | orcs/sensors/ph/0             |
| Dissolved Oxygen    | orcs/sensors/do            | orcs/sensors/do/0             |
| Optical Density     | orcs/sensors/od            | orcs/sensors/od/0             |
| Gas Flow            | orcs/sensors/gasflow       | orcs/sensors/gasflow/0        |
| Pressure            | orcs/sensors/pressure      | orcs/sensors/pressure/0       |
| Stirrer Speed       | orcs/sensors/stirrer       | orcs/sensors/stirrer/0        |
| Weight              | orcs/sensors/weight        | orcs/sensors/weight/0         |

### 2. System Status Data (from RP2040 Controller)
| Data Type           | Topic                           | Description                    |
|---------------------|--------------------------------|--------------------------------|
| Power Voltage       | orcs/system/power/voltage      | Main PSU voltage (V)           |
| 20V Rail            | orcs/system/power/20v          | 20V rail voltage (V)           |
| 5V Rail             | orcs/system/power/5v           | 5V rail voltage (V)            |
| PSU Status          | orcs/system/status/psu_ok      | PSU OK status (1=OK, 0=Fault) |
| 20V Status          | orcs/system/status/20v_ok      | 20V rail OK (1=OK, 0=Fault)   |
| 5V Status           | orcs/system/status/5v_ok       | 5V rail OK (1=OK, 0=Fault)    |
| SD Card Status      | orcs/system/status/sdcard_ok   | SD card OK (1=OK, 0=Fault)    |
| IPC Status          | orcs/system/status/ipc_ok      | IPC OK (1=OK, 0=Fault)        |
| RTC Status          | orcs/system/status/rtc_ok      | RTC OK (1=OK, 0=Fault)        |
| Modbus Connection   | orcs/system/status/modbus_connected | Modbus connected (1=Connected, 0=Not) |
| Modbus Busy         | orcs/system/status/modbus_busy | Modbus busy (1=Busy, 0=Idle)  |
| Webserver Status    | orcs/system/status/webserver_up | Webserver up (1=Up, 0=Down)   |
| Webserver Busy      | orcs/system/status/webserver_busy | Webserver busy (1=Busy, 0=Idle) |
| MQTT Connection     | orcs/system/status/mqtt_connected | MQTT connected (1=Connected, 0=Not) |
| MQTT Busy           | orcs/system/status/mqtt_busy   | MQTT busy (1=Busy, 0=Idle)    |

### 3. Consolidated Data Topics
| Topic                    | Description                                    |
|--------------------------|------------------------------------------------|
## MQTT Topic Structure

Topics are scoped per device using a prefix:

- Default: `orcs/dev/<MAC>` (e.g., `orcs/dev/AA:BB:CC:DD:EE:FF`)
- Override: `mqttDevicePrefix` in network config (`/api/mqtt`)

All topics below are relative to the device prefix unless stated as absolute.

The ORC system publishes two types of sensor data:
| orcs/system/sensors      | All system status data in one JSON payload    |

### 1. External Sensor Data (from I/O Controller via IPC)
| Sensor Type         | Topic Prefix (relative)     | Example Topic                  |
---
| Power               | orcs/sensors/power         | orcs/dev/<MAC>/orcs/sensors/power/0          |
| Temperature         | orcs/sensors/temperature   | orcs/dev/<MAC>/orcs/sensors/temperature/0    |
| pH                  | orcs/sensors/ph            | orcs/dev/<MAC>/orcs/sensors/ph/0             |
| Dissolved Oxygen    | orcs/sensors/do            | orcs/dev/<MAC>/orcs/sensors/do/0             |
| Optical Density     | orcs/sensors/od            | orcs/dev/<MAC>/orcs/sensors/od/0             |
| Gas Flow            | orcs/sensors/gasflow       | orcs/dev/<MAC>/orcs/sensors/gasflow/0        |
| Pressure            | orcs/sensors/pressure      | orcs/dev/<MAC>/orcs/sensors/pressure/0       |
| Stirrer Speed       | orcs/sensors/stirrer       | orcs/dev/<MAC>/orcs/sensors/stirrer/0        |
| Weight              | orcs/sensors/weight        | orcs/dev/<MAC>/orcs/sensors/weight/0         |

### External Sensor Data (IPC)
### 2. System Status Data (from RP2040 Controller)
| Data Type           | Topic (relative)                | Description                    |
- Data is received from the I/O Controller via IPC and immediately published to the MQTT broker.
| Power Voltage       | sensors/power/voltage          | Main PSU voltage (V)           |
| 20V Rail            | sensors/power/20v              | 20V rail voltage (V)           |
| 5V Rail             | sensors/power/5v               | 5V rail voltage (V)            |
| PSU Status          | status/psu_ok                  | PSU OK status (1=OK, 0=Fault) |
| 20V Status          | status/20v_ok                  | 20V rail OK (1=OK, 0=Fault)   |
| 5V Status           | status/5v_ok                   | 5V rail OK (1=OK, 0=Fault)    |
| SD Card Status      | status/sdcard_ok               | SD card OK (1=OK, 0=Fault)    |
| IPC Status          | status/ipc_ok                  | IPC OK (1=OK, 0=Fault)        |
| RTC Status          | status/rtc_ok                  | RTC OK (1=OK, 0=Fault)        |
| Modbus Connection   | status/modbus_connected        | Modbus connected (1=Connected, 0=Not) |
| Modbus Busy         | status/modbus_busy             | Modbus busy (1=Busy, 0=Idle)  |
| Webserver Status    | status/webserver_up            | Webserver up (1=Up, 0=Down)   |
| Webserver Busy      | status/webserver_busy          | Webserver busy (1=Busy, 0=Idle) |
| MQTT Connection     | status/mqtt_connected          | MQTT connected (1=Connected, 0=Not) |
| MQTT Busy           | status/mqtt_busy               | MQTT busy (1=Busy, 0=Idle)    |

## Example MQTT Payloads

### Individual Sensor Topic

### Individual Sensor Topic

**Note:** Sensor names are encoded in the MQTT topic path, not the payload. For example:

Topic: `orcs/dev/<MAC>/sensors/temperature/0`
Payload:
```json
{
  "timestamp": "2025-07-19T10:30:00Z",
  "value": 37.50,
  "online": true
}
```

If you want the sensor name in the payload, add a `"name"` field:
```json
{
  "timestamp": "2025-07-19T10:30:00Z",
  "value": 37.50,
  "online": true,
  "name": "temperature"
}
```

### Consolidated System Status Topic (`<devicePrefix>/sensors/all`)
Payload example:
```json
{
  "sensors": {
    "sensors/power/voltage": { "value": 23.62, "timestamp": "2025-07-18T14:23:45Z" },
    "sensors/power/20v": { "value": 19.93, "timestamp": "2025-07-18T14:23:45Z" },
    "status/psu_ok": { "value": 1.00, "timestamp": "2025-07-18T14:23:45Z" }
  }
}
```

## Data Flow Summary

### Subscribe to all external sensor data (all devices):
### External Sensors (via IPC)
mosquitto_sub -h broker -t "orcs/dev/+/orcs/sensors/#"
2. Data is sent to the System Controller (RP2040) via IPC.
3. RP2040 formats the data and publishes it to the MQTT broker on `orcs/sensors/` topics.
### Subscribe to all system status data for one device:
4. Each sensor type and instance is mapped to a unique topic for easy subscription and filtering.
mosquitto_sub -h broker -t "orcs/dev/<MAC>/status/#"
### System Status (RP2040 Local)
1. RP2040 controller monitors its own systems (power, network, storage, etc.).
### Subscribe to consolidated system status for one device:
2. Data is collected and formatted locally.
mosquitto_sub -h broker -t "orcs/dev/<MAC>/sensors/all"

---
### Subscribe to everything (all devices):

mosquitto_sub -h broker -t "orcs/dev/#"

### External Sensor Registry (IPC)
### Example Command to list all topics if IP is 10.10.37.118 (all devices)
```cpp
mosquitto_sub -h 10.10.37.118 -p 1883 -t "orcs/dev/#" -v
---

## Reliability Features

- Last Will and Testament (LWT): `<devicePrefix>/status/online` is retained `true` when connected; broker sets `false` on unexpected disconnect.
- KeepAlive: 30s; socket timeout: 15s; reconnect backoff: 5s.
- Buffer size: 1024 bytes to support JSON payloads.
- Configurable publish interval via `/api/mqtt` (`mqttPublishIntervalMs`).
    {MSG_POWER_SENSOR, "orcs/sensors/power"},
    {MSG_TEMPERATURE_SENSOR, "orcs/sensors/temperature"},
    {MSG_PH_SENSOR, "orcs/sensors/ph"},
    {MSG_DO_SENSOR, "orcs/sensors/do"},
    {MSG_OD_SENSOR, "orcs/sensors/od"},
    {MSG_GAS_FLOW_SENSOR, "orcs/sensors/gasflow"},
    {MSG_PRESSURE_SENSOR, "orcs/sensors/pressure"},
    {MSG_STIRRER_SPEED_SENSOR, "orcs/sensors/stirrer"},
    {MSG_WEIGHT_SENSOR, "orcs/sensors/weight"}
};
```

### System Status Registry (RP2040)
```cpp
MqttTopicEntry mqttTopics[] = {
    {"orcs/system/power/voltage", getVpsu, "Main PSU voltage (V)"},
    {"orcs/system/power/20v", getV20, "20V rail voltage (V)"},
    {"orcs/system/power/5v", getV5, "5V rail voltage (V)"},
    {"orcs/system/status/psu_ok", getPsuOK, "PSU OK status (1=OK, 0=Fault)"},
    // ... more status topics
};
```

---

## Notes
- All payloads are JSON and use ISO 8601 timestamps.
- **External sensor topics** (`orcs/sensors/`) are for actual sensor readings from the I/O controller.
- **System status topics** (`orcs/system/`) are for RP2040 controller health and status monitoring.
- Topics are designed for scalability and easy integration with external systems.
- The registry makes it simple to add new sensor types or instances.
- Both individual topics and consolidated topics are published for flexibility.

## Subscription Examples

### Subscribe to all external sensor data:
```bash
mosquitto_sub -h broker -t "orcs/sensors/#"
```

### Subscribe to all system status data:
```bash
mosquitto_sub -h broker -t "orcs/system/#"
```

### Subscribe to consolidated system status:
```bash
mosquitto_sub -h broker -t "orcs/system/sensors"
```

### Subscribe to everything:
```bash
mosquitto_sub -h broker -t "orcs/#"
```


### Example Command to list all topics if IP is 10.10.37.118
```bash
mosquitto_sub -h 10.10.37.118 -p 1883 -t "orcs/#" -v
```