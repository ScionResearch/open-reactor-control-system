# MQTT API Reference

This document describes the MQTT interface for the Open Reactor Control System (ORCS).

## Connection Details

| Parameter | Default | Description |
|-----------|---------|-------------|
| Broker | Configurable via web UI | MQTT broker hostname/IP |
| Port | 1883 | Standard MQTT port |
| Username | Optional | Authentication username |
| Password | Optional | Authentication password |

## Topic Structure

All topics use the device MAC address as a unique identifier:

```
orc/{MAC}/data/...     # Published sensor/status data (subscribe to receive)
orc/{MAC}/control/...  # Control commands (publish to send commands)
```

**Example MAC:** `2E:45:83:40:5A:22`

---

## Data Topics (Published by Device)

The device publishes sensor data and status updates to these topics. Subscribe to receive real-time data.

### Sensor Data Format

All sensor data is published as JSON with a consistent structure:

```json
{
  "timestamp": "2025-01-15T14:23:45Z",
  "name": "Reactor Temperature",
  "value": 37.5,
  "unit": "°C",
  "fault": false,
  "message": "Optional status message"
}
```

### Data Topic Paths

| Topic | Description | Value Type |
|-------|-------------|------------|
| `orc/{MAC}/data/sensors/temperature/{index}` | Temperature sensors (1-7) | °C |
| `orc/{MAC}/data/sensors/adc/{index}` | ADC inputs (8-9) | mV |
| `orc/{MAC}/data/sensors/digital/{index}` | Digital inputs (10-20) | boolean |
| `orc/{MAC}/data/outputs/digital/{index}` | Digital outputs (21-25) | state, PWM% |
| `orc/{MAC}/data/outputs/stepper` | Stepper motor (26) | RPM, running |
| `orc/{MAC}/data/outputs/dcmotor/{index}` | DC motors (27-30) | power%, running |
| `orc/{MAC}/data/controllers/temperature/{index}` | Temp controllers (40-42) | setpoint, PV, output |
| `orc/{MAC}/data/controllers/ph` | pH controller (43) | setpoint, PV, volumes |
| `orc/{MAC}/data/controllers/flow/{index}` | Flow controllers (44-47) | flowrate, volume |
| `orc/{MAC}/data/controllers/do` | DO controller (48) | setpoint, PV |
| `orc/{MAC}/data/devices/{index}` | MFC/devices (50-69) | setpoint, flow, pressure |

### Controller Data Example

Temperature controller publishes:
```json
{
  "timestamp": "2025-01-15T14:23:45Z",
  "name": "Jacket Heater",
  "enabled": true,
  "setpoint": 37.0,
  "pv": 36.8,
  "output": 45.2,
  "method": "PID",
  "kP": 10.0,
  "kI": 0.5,
  "kD": 1.0
}
```

---

## Control Topics (Commands to Device)

Publish JSON commands to these topics to control the device.

### Acknowledgment

After each command, the device publishes an acknowledgment to `{control_topic}/ack`:

```json
{
  "success": true,
  "message": "Output command sent",
  "timestamp": "2025-01-15T14:23:45Z"
}
```

---

## Control Command Reference

### Digital Outputs (21-25)

**Topic:** `orc/{MAC}/control/output/{index}`

| Command | Payload | Description |
|---------|---------|-------------|
| Set State | `{"state": true}` | Turn output ON/OFF |
| Set PWM | `{"power": 50}` | Set PWM duty cycle (0-100%) |

**Example:**
```bash
mosquitto_pub -t "orc/2E:45:83:40:5A:22/control/output/21" -m '{"state": true}'
```

---

### DAC Outputs (8-9)

**Topic:** `orc/{MAC}/control/output/{index}`

| Command | Payload | Description |
|---------|---------|-------------|
| Set Voltage | `{"mV": 2500}` | Set output voltage (0-10240 mV) |

**Example:**
```bash
mosquitto_pub -t "orc/2E:45:83:40:5A:22/control/output/8" -m '{"mV": 5000}'
```

---

### Stepper Motor (26)

**Topic:** `orc/{MAC}/control/stepper`

| Command | Payload | Description |
|---------|---------|-------------|
| Start | `{"start": true, "rpm": 100, "forward": true}` | Start motor at specified RPM and direction |
| Stop | `{"stop": true}` | Stop motor |
| Set RPM | `{"rpm": 150}` | Change RPM while running |
| Set Direction | `{"forward": false}` | Change direction (true=forward, false=reverse) |

**Example:**
```bash
mosquitto_pub -t "orc/2E:45:83:40:5A:22/control/stepper" -m '{"start": true, "rpm": 200, "forward": true}'
```

---

### DC Motors (27-30)

**Topic:** `orc/{MAC}/control/motor/{index}`

| Command | Payload | Description |
|---------|---------|-------------|
| Start | `{"start": true, "power": 50, "forward": true}` | Start motor at specified power and direction |
| Stop | `{"stop": true}` | Stop motor |
| Set Power | `{"power": 75}` | Change power while running (0-100%) |
| Set Direction | `{"forward": false}` | Change direction |

**Example:**
```bash
mosquitto_pub -t "orc/2E:45:83:40:5A:22/control/motor/27" -m '{"start": true, "power": 80, "forward": true}'
```

---

### MFC Devices (50-69)

**Topic:** `orc/{MAC}/control/device/{index}`

| Command | Payload | Description |
|---------|---------|-------------|
| Set Setpoint | `{"setpoint": 100.0}` | Set flow setpoint (device units) |

**Example:**
```bash
mosquitto_pub -t "orc/2E:45:83:40:5A:22/control/device/50" -m '{"setpoint": 50.0}'
```

---

### Temperature Controllers (40-42)

**Topic:** `orc/{MAC}/control/controller/temp/{index}`

| Command | Payload | Description |
|---------|---------|-------------|
| Enable | `{"enabled": true}` | Enable controller |
| Disable | `{"enabled": false}` | Disable controller |
| Set Setpoint | `{"setpoint": 37.5}` | Set target temperature |
| Start Autotune | `{"autotune": true}` | Start PID autotune |
| Set PID | `{"kp": 10, "ki": 0.5, "kd": 1}` | Update PID parameters |
| Set Hysteresis | `{"hysteresis": 0.5}` | Set On/Off hysteresis |

**Example:**
```bash
mosquitto_pub -t "orc/2E:45:83:40:5A:22/control/controller/temp/40" -m '{"setpoint": 37.0}'
```

---

### pH Controller (43)

**Topic:** `orc/{MAC}/control/controller/ph/43`

| Command | Payload | Description |
|---------|---------|-------------|
| Enable | `{"enabled": true}` | Enable controller |
| Disable | `{"enabled": false}` | Disable controller |
| Set Setpoint | `{"setpoint": 7.0}` | Set target pH (0-14) |
| Dose Acid | `{"doseAcid": true}` | Trigger single acid dose |
| Dose Alkaline | `{"doseAlkaline": true}` | Trigger single base dose |
| Reset Volumes | `{"resetVolumes": true}` | Reset cumulative volume counters |

**Example:**
```bash
mosquitto_pub -t "orc/2E:45:83:40:5A:22/control/controller/ph/43" -m '{"setpoint": 7.2}'
```

---

### Flow Controllers (44-47)

**Topic:** `orc/{MAC}/control/controller/flow/{index}`

| Command | Payload | Description |
|---------|---------|-------------|
| Enable | `{"enabled": true}` | Enable controller |
| Disable | `{"enabled": false}` | Disable controller |
| Set Flow Rate | `{"setpoint": 10.0}` | Set target flow rate (mL/min) |
| Manual Dose | `{"manualDose": true}` | Trigger single dose cycle |
| Reset Volume | `{"resetVolume": true}` | Reset cumulative volume counter |

**Indices:**
- 44-46: Feed Pumps 1-3
- 47: Waste Pump

**Example:**
```bash
mosquitto_pub -t "orc/2E:45:83:40:5A:22/control/controller/flow/44" -m '{"setpoint": 5.0}'
```

---

### DO Controller (48)

**Topic:** `orc/{MAC}/control/controller/do/48`

| Command | Payload | Description |
|---------|---------|-------------|
| Enable | `{"enabled": true}` | Enable controller |
| Disable | `{"enabled": false}` | Disable controller |
| Set Setpoint | `{"setpoint": 40.0}` | Set target DO (% saturation or mg/L) |
| Set Profile (by index) | `{"activeProfileIndex": 0}` | Select profile by index (0-2) |
| Set Profile (by name) | `{"activeProfile": "Cascade"}` | Select profile by name |

**Example:**
```bash
mosquitto_pub -t "orc/2E:45:83:40:5A:22/control/controller/do/48" -m '{"setpoint": 30.0}'
```

---

## Object Index Reference

| Index Range | Object Type |
|-------------|-------------|
| 1-7 | Temperature Sensors (PT100/PT1000) |
| 8-9 | ADC Inputs / DAC Outputs |
| 10-20 | Digital Inputs |
| 21-25 | Digital Outputs (PWM capable) |
| 26 | Stepper Motor |
| 27-30 | DC Motors |
| 40-42 | Temperature Controllers |
| 43 | pH Controller |
| 44-47 | Flow Controllers |
| 48 | Dissolved Oxygen Controller |
| 50-69 | Dynamic Devices (MFC, probes, etc.) |

---

## Error Handling

If a command fails, the acknowledgment will indicate the error:

```json
{
  "success": false,
  "message": "Invalid JSON",
  "timestamp": "2025-01-15T14:23:45Z"
}
```

Common error messages:
- `"Invalid JSON"` - Malformed JSON payload
- `"Invalid topic format"` - Topic path not recognized
- `"Unknown control category"` - Category not supported
- `"IPC queue full"` - Internal communication busy, retry

---

## QoS and Retention

- **Data topics:** Published with QoS 0, no retention
- **Control topics:** Recommend QoS 1 for reliability
- **LWT (Last Will):** Device publishes offline status on disconnect

---

## Example Integration (Python)

```python
import paho.mqtt.client as mqtt
import json

MAC = "2E:45:83:40:5A:22"
BROKER = "192.168.1.100"

def on_message(client, userdata, msg):
    data = json.loads(msg.payload)
    print(f"{msg.topic}: {data}")

client = mqtt.Client()
client.on_message = on_message
client.connect(BROKER, 1883)

# Subscribe to all data
client.subscribe(f"orc/{MAC}/data/#")

# Send a control command
client.publish(
    f"orc/{MAC}/control/output/21",
    json.dumps({"state": True})
)

client.loop_forever()
```

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-11-30 | Initial MQTT control implementation |
