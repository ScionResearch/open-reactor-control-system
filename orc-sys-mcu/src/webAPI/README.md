# Web API Documentation

This document provides a quick reference for the web API module structure and guidelines for adding new endpoints.

## Architecture Overview

The web API is organized into domain-specific modules, each responsible for a logical group of endpoints. All modules are initialized from `webServer.cpp`, which acts as the central coordinator.

```
webAPI/
├── webServer.cpp/h      # Central server setup and static file serving
├── apiSystem.cpp/h      # System status, sensors, recording, backup/restore
├── apiNetwork.cpp/h     # Network configuration (IP, DHCP, hostname)
├── apiTime.cpp/h        # NTP and time synchronization
├── apiMqtt.cpp/h        # MQTT broker configuration
├── apiInputs.cpp/h      # ADC, DAC, RTD, GPIO, energy sensors, device sensors
├── apiOutputs.cpp/h     # Digital outputs, motor outputs
├── apiControllers.cpp/h # PID controllers (temp, pH, flow, DO)
├── apiDevices.cpp/h     # External devices (Modbus, etc.)
└── apiFileManager.cpp/h # SD card file operations
```

## Module Pattern

Each API module follows a consistent pattern:

### Header File (`api*.h`)
```cpp
#pragma once

/**
 * @file apiExample.h
 * @brief Brief description of what this module handles
 * 
 * Handles:
 * - /api/example - Endpoint description
 * - /api/example/config - Another endpoint
 */

#include <Arduino.h>

// Setup function - called from webServer.cpp
void setupExampleAPI(void);

// Handler functions
void handleGetExample(void);
void handleSaveExample(void);
```

### Implementation File (`api*.cpp`)
```cpp
#include "apiExample.h"
#include "../network/networkManager.h"  // For 'server' object
#include <ArduinoJson.h>

extern WebServer server;

void setupExampleAPI() {
    server.on("/api/example", HTTP_GET, handleGetExample);
    server.on("/api/example", HTTP_POST, handleSaveExample);
}

void handleGetExample() {
    DynamicJsonDocument doc(1024);
    doc["value"] = 42;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveExample() {
    // Parse incoming JSON
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    // Process and save...
    server.send(200, "application/json", "{\"success\":true}");
}
```

## Endpoint Index

### System (`apiSystem.cpp`)
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/system/status` | System status (uptime, memory, etc.) |
| GET | `/api/status/all` | Aggregated status of all subsystems |
| GET | `/api/sensors` | All sensor readings |
| POST | `/api/reboot` | Trigger system reboot |
| GET | `/api/recording/config` | Get recording configuration |
| POST | `/api/recording/config` | Save recording configuration |
| GET | `/api/config/backup` | Export full configuration backup |
| POST | `/api/config/restore` | Restore configuration from backup |
| POST | `/api/config/backup/sd` | Save backup to SD card |
| GET | `/api/config/backup/sd/list` | List backups on SD card |

### Network (`apiNetwork.cpp`)
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/network/config` | Get network configuration |
| POST | `/api/network/config` | Save network configuration |

### Time (`apiTime.cpp`)
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/time` | Get current time |
| POST | `/api/time` | Set time manually |
| GET | `/api/ntp/config` | Get NTP configuration |
| POST | `/api/ntp/config` | Save NTP configuration |

### MQTT (`apiMqtt.cpp`)
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/mqtt/config` | Get MQTT configuration |
| POST | `/api/mqtt/config` | Save MQTT configuration |

### Inputs (`apiInputs.cpp`)
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/inputs` | Get all input values |
| GET | `/api/input/{index}/config` | Get input configuration |
| POST | `/api/input/{index}/config` | Save input configuration |
| GET | `/api/comports` | Get COM port list |
| GET | `/api/comport/{index}/config` | Get COM port configuration |
| POST | `/api/comport/{index}/config` | Save COM port configuration |

**Input Index Ranges:**
- `0-7`: ADC channels
- `8-9`: DAC channels (readback)
- `10-12`: RTD temperature inputs
- `13-20`: GPIO digital inputs
- `31-32`: Energy sensor inputs
- `70-99`: Device sensor inputs

### Outputs (`apiOutputs.cpp`)
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/outputs` | Get all output states |
| GET | `/api/output/{index}/config` | Get output configuration |
| POST | `/api/output/{index}/config` | Save output configuration |
| POST | `/api/output/{index}/set` | Set output value |

**Output Index Ranges:**
- `0-7`: Digital outputs
- `10-13`: Motor/pump outputs

### Controllers (`apiControllers.cpp`)
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/controllers` | Get all controller states |
| GET | `/api/controller/{index}` | Get controller details |
| POST | `/api/controller/{index}` | Update controller config |
| DELETE | `/api/controller/{index}` | Delete controller |
| POST | `/api/controller/{index}/setpoint` | Set controller setpoint |
| POST | `/api/controller/{index}/enable` | Enable/disable controller |
| POST | `/api/controller/{index}/autotune` | Start autotune |
| POST | `/api/controllers/add` | Create new controller |

**Controller Index Ranges:**
- `40-42`: Temperature controllers
- `43`: pH controller
- `44-47`: Flow controllers
- `48`: Dissolved oxygen controller

### Devices (`apiDevices.cpp`)
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/devices` | Get all devices |
| GET | `/api/devices/{index}` | Get device details |
| POST | `/api/devices/{index}` | Update device config |
| DELETE | `/api/devices/{index}` | Delete device |
| POST | `/api/devices/add` | Create new device |
| GET | `/api/devices/control` | Get device control states |
| POST | `/api/device/{index}/setpoint` | Send setpoint to device |

**Device Index Range:** `50-69`

### File Manager (`apiFileManager.cpp`)
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/files` | List files in directory |
| GET | `/api/files/download` | Download a file |
| POST | `/api/files/upload` | Upload a file |
| DELETE | `/api/files/delete` | Delete a file |
| POST | `/api/files/mkdir` | Create directory |

## Dynamic Routes

Some endpoints use dynamic path parameters (e.g., `/api/controller/{index}`). These are handled in `webServer.cpp` via the `onNotFound` handler, which parses the URI and dispatches to the appropriate handler:

```cpp
server.onNotFound([]() {
    String uri = server.uri();
    
    if (uri.startsWith("/api/devices/") && uri.length() > 13) {
        handleDynamicDeviceRoute();
        return;
    }
    
    if (uri.startsWith("/api/controller/") && uri.length() > 16) {
        handleDynamicControllerRoute();
        return;
    }
    
    // ... other dynamic routes
    
    // Fall back to static file serving
    handleFile(uri.c_str());
});
```

## Adding New Endpoints

### 1. Simple Endpoint (Static Route)

Add directly in the module's `setup*API()` function:

```cpp
void setupMyAPI() {
    server.on("/api/my/endpoint", HTTP_GET, handleMyEndpoint);
}
```

### 2. Parameterized Endpoint (Dynamic Route)

1. Add handler function to appropriate module
2. Add route matching in `webServer.cpp` `onNotFound` handler
3. Parse parameters from URI in the handler

### 3. New Module

1. Create `apiNewModule.h` and `apiNewModule.cpp`
2. Follow the module pattern above
3. Add `#include "apiNewModule.h"` in `webServer.cpp`
4. Call `setupNewModuleAPI()` in `setupWebServer()`

## Response Conventions

### Success Response
```json
{
    "success": true,
    "message": "Operation completed"
}
```

### Error Response
```json
{
    "error": "Description of what went wrong"
}
```

### Data Response
```json
{
    "fieldName": "value",
    "nestedObject": {
        "subField": 123
    }
}
```

## Common Dependencies

All API modules typically include:

```cpp
#include "../network/networkManager.h"  // WebServer 'server' object
#include "../utils/logger.h"            // Logging macros
#include "../config/ioConfig.h"         // IO configuration data
#include <ArduinoJson.h>                // JSON serialization
```

## JSON Document Sizing

Use appropriate document sizes based on response complexity:

| Size | Use Case |
|------|----------|
| 256-512 | Simple config (single input/output) |
| 1024-2048 | Medium config (controller, device) |
| 4096-8192 | Large lists (all inputs, all outputs) |
| 16384+ | Full configuration backup |

## Error Handling

Always validate:
1. Request method matches expected
2. JSON parsing succeeds
3. Required fields are present
4. Index values are in valid range
5. Referenced resources exist

```cpp
void handleExample() {
    // Validate JSON
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, server.arg("plain"))) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    // Validate required field
    if (!doc.containsKey("value")) {
        server.send(400, "application/json", "{\"error\":\"Missing required field: value\"}");
        return;
    }
    
    // Validate range
    int index = doc["index"] | -1;
    if (index < 0 || index > 99) {
        server.send(400, "application/json", "{\"error\":\"Index out of range\"}");
        return;
    }
    
    // Process...
}
```
