# MQTT Topics Registry

This document lists all MQTT topics published by the System MCU (RP2040) and their payload structures. Each topic corresponds to a sensor value or system variable, and is published every 10 seconds.

| Topic                                   | Description                              | Payload Example                                  |
|-----------------------------------------|------------------------------------------|--------------------------------------------------|
| orcs/system/power/voltage               | Main PSU voltage (V)                     | `{ "value": 24.15, "timestamp": "2025-07-15T10:30:00Z" }` |
| orcs/system/power/20v                   | 20V rail voltage (V)                     | `{ "value": 20.01, "timestamp": "2025-07-15T10:30:00Z" }` |
| orcs/system/power/5v                    | 5V rail voltage (V)                      | `{ "value": 5.02,  "timestamp": "2025-07-15T10:30:00Z" }` |
| orcs/system/status/psu_ok               | PSU OK status (1=OK, 0=Fault)            | `{ "value": 1, "timestamp": "2025-07-15T10:30:00Z" }` |
| orcs/system/status/20v_ok               | 20V rail OK status (1=OK, 0=Fault)       | `{ "value": 1, "timestamp": "2025-07-15T10:30:00Z" }` |
| orcs/system/status/5v_ok                | 5V rail OK status (1=OK, 0=Fault)        | `{ "value": 1, "timestamp": "2025-07-15T10:30:00Z" }` |
| orcs/system/status/sdcard_ok            | SD card OK status (1=OK, 0=Fault)        | `{ "value": 1, "timestamp": "2025-07-15T10:30:00Z" }` |
| orcs/system/status/ipc_ok               | IPC OK status (1=OK, 0=Fault)            | `{ "value": 1, "timestamp": "2025-07-15T10:30:00Z" }` |
| orcs/system/status/rtc_ok               | RTC OK status (1=OK, 0=Fault)            | `{ "value": 1, "timestamp": "2025-07-15T10:30:00Z" }` |
| orcs/system/status/modbus_connected     | Modbus connected (1=Connected, 0=Not)    | `{ "value": 1, "timestamp": "2025-07-15T10:30:00Z" }` |
| orcs/system/status/modbus_busy          | Modbus busy (1=Busy, 0=Idle)             | `{ "value": 0, "timestamp": "2025-07-15T10:30:00Z" }` |
| orcs/system/status/webserver_up         | Webserver up (1=Up, 0=Down)              | `{ "value": 1, "timestamp": "2025-07-15T10:30:00Z" }` |
| orcs/system/status/webserver_busy       | Webserver busy (1=Busy, 0=Idle)          | `{ "value": 0, "timestamp": "2025-07-15T10:30:00Z" }` |
| orcs/system/status/mqtt_connected       | MQTT connected (1=Connected, 0=Not)      | `{ "value": 1, "timestamp": "2025-07-15T10:30:00Z" }` |
| orcs/system/status/mqtt_busy            | MQTT busy (1=Busy, 0=Idle)               | `{ "value": 0, "timestamp": "2025-07-15T10:30:00Z" }` |

## Payload Structure

All topics use the following JSON structure:

```json
{
  "value": <float>,
  "timestamp": "<ISO8601 UTC>"
}
```
- `value`: The measured sensor value (floating point).
- `timestamp`: ISO 8601 UTC timestamp of the measurement.

## Adding New Topics
To add a new sensor or system variable for MQTT publication:
1. Implement a getter function that returns the value as a `float`.
2. Add a new entry to the `mqttTopics` array in `mqttManager.cpp` with the topic name, getter, and description.
3. Update this documentation table.

## Future Expansion
When IO MCU data is available, add new topics and getter functions as needed. This registry is the single source of truth for all MQTT data published by the System MCU.
