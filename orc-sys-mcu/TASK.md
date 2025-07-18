### **1. High-Level Analysis and Plan**

#### **Feature Interpretation**

The goal is to overhaul the user interface by:
1.  **Creating a dynamic dashboard:** This main page will display real-time values for all system sensors and provide input fields to modify control setpoints.
2.  **Reorganizing the UI:** The existing data graph will be moved from the main page to a new, dedicated "Graphs" tab, creating a cleaner, more organized, tab-based navigation system.

This feature requires a full-stack implementation, including new backend API endpoints for control, modifications to the frontend (HTML, CSS, JS), and logic to manage the flow of data from the I/O controller to the UI.

#### **Architectural Placement & Data Flow**

The feature logic will be implemented primarily on the **System Controller (RP2040)**, which orchestrates UI, network, and high-level control.

*   **System Controller (RP2040) Logic:**
    *   **New Control Manager (`controlManager`):** A new module will be created to handle incoming control commands from the API. Its sole responsibility is to receive parsed setpoint data, construct the appropriate IPC message (e.g., `TemperatureControl`), and send it to the I/O Controller.
    *   **Status Manager (`statusManager`):** The global `StatusVariables` struct will be expanded to hold the latest values for all sensors and control setpoints. This creates a single source of truth for the system's state.
    *   **IPC Manager (`ipcManager`):** The IPC callback functions will be updated. When a sensor data message arrives from the I/O Controller, the callback will now perform two actions: update the global `status` struct and publish the data to MQTT.
    *   **Network Manager (`network.cpp`):**
        *   It will be modified to serve the new tabbed HTML layout and updated JavaScript/CSS.
        *   A new comprehensive API endpoint, `GET /api/status/all`, will be created to return the entire `status` struct as a single JSON object.
        *   A new scalable API endpoint, `POST /api/controls`, will be added to receive setpoint update commands from the UI.
    *   **Frontend (HTML/JS/CSS):** A complete redesign of the web interface will be implemented with tab navigation, a grid-based dashboard, and interactive controls.

*   **I/O Controller (SAMD51) Logic (Conceptual):**
    *   No code changes are required on the I/O Controller. It will continue to send sensor data periodically via IPC. It will also receive and execute control setpoint commands (e.g., `MSG_TEMPERATURE_CONTROL`) sent by the RP2040's new `controlManager`.

*   **IPC Protocol (`IPCDataStructs.h`):**
    *   No changes are required. The existing sensor and control data structures are sufficient for this feature.

*   **New Data Flow (Mermaid Diagram):**

    ```mermaid
    sequenceDiagram
        participant UI Browser
        participant RP2040 Web Server
        participant RP2040 Control/Status Mgr
        participant RP2040 IPC Layer
        participant I/O Controller (SAMD51)

        Note over UI Browser, I/O Controller (SAMD51): Data Display Flow
        I/O Controller (SAMD51)->>+RP2040 IPC Layer: IPC Message (e.g., TemperatureSensor)
        RP2040 IPC Layer->>+RP2040 Control/Status Mgr: IPC callback updates global `status` struct
        deactivate RP2040 IPC Layer
        deactivate RP2040 Control/Status Mgr

        UI Browser->>+RP2040 Web Server: Periodic GET /api/status/all
        RP2040 Web Server->>+RP2040 Control/Status Mgr: Reads global `status` struct
        RP2040 Control/Status Mgr-->>-RP2040 Web Server: Returns full status JSON
        RP2040 Web Server-->>-UI Browser: Responds with JSON data
        UI Browser->>UI Browser: Updates Dashboard cards

        Note over UI Browser, I/O Controller (SAMD51): Setpoint Update Flow
        UI Browser->>+RP2040 Web Server: User clicks 'Save', sends POST /api/controls with JSON payload
        RP2040 Web Server->>+RP2040 Control/Status Mgr: API handler calls controlManager function
        RP2040 Control/Status Mgr->>RP2040 Control/Status Mgr: Updates setpoint in global `status` struct
        RP2040 Control/Status Mgr->>+RP2040 IPC Layer: Sends IPC Message (e.g., TemperatureControl)
        RP2040 IPC Layer->>+I/O Controller (SAMD51): Delivers control command
        I/O Controller (SAMD51)-->>-I/O Controller (SAMD51): Adjusts its internal PID loop setpoint
        deactivate I/O Controller (SAMD51)
        deactivate RP2040 IPC Layer
        deactivate RP2040 Control/Status Mgr
        RP2040 Web Server-->>-UI Browser: Responds with {success: true}
        deactivate RP2040 Web Server
    ```

#### **Impact Summary**

*   `src/utils/statusManager.h`: **Modified** to expand `StatusVariables`.
*   `src/utils/ipcManager.cpp`: **Modified** to update the global status struct from IPC messages.
*   `src/sys_init.h`, `src/sys_init.cpp`: **Modified** to include and initialize the new `controlManager`.
*   `src/network/network.cpp`: **Modified** to add the new `GET /api/status/all` and `POST /api/controls` endpoints.
*   `src/controls/controlManager.h`: **New File**.
*   `src/controls/controlManager.cpp`: **New File**.
*   `data/index.html`: **Replaced** with a new tab-based structure.
*   `data/script.js`: **Replaced** with new logic for tabs, data fetching, and control submissions.
*   `data/style.css`: **Replaced** with new styles for the modern UI layout.

---

### **2. Complete Implementation & Code Generation**

#### **Backend (C++)**

##### **1. Create New File: `src/controls/controlManager.h`**

This header defines the interface for updating control parameters.

```cpp
// src/controls/controlManager.h
#pragma once

#include "../sys_init.h"

// Initialize the control manager
void init_controlManager();

// Functions to update control parameters based on API requests
bool updateTemperatureControl(const JsonObject& config);
bool updatePhControl(const JsonObject& config);
// Add other control update functions here as needed, e.g.:
// bool updateStirrerControl(const JsonObject& config);
```

##### **2. Create New File: `src/controls/controlManager.cpp`**

This file implements the logic to create and send IPC messages for control updates.

```cpp
// src/controls/controlManager.cpp
#include "controlManager.h"
#include "../utils/statusManager.h"
#include "../utils/logger.h"
#include "lib/IPCprotocol/IPCProtocol.h"

void init_controlManager() {
    log(LOG_INFO, false, "Control Manager initialized.\n");
}

/**
 * @brief Updates temperature control settings and sends an IPC message.
 * @param config A JsonObject containing "setpoint" and "enabled".
 * @return True on success, false on failure.
 */
bool updateTemperatureControl(const JsonObject& config) {
    if (!config.containsKey("setpoint") || !config.containsKey("enabled")) {
        log(LOG_WARNING, true, "API: Invalid temperature control payload.\n");
        return false;
    }

    float setpoint = config["setpoint"];
    bool enabled = config["enabled"];

    // 1. Prepare IPC message
    Message msg;
    msg.msgId = MSG_TEMPERATURE_CONTROL;
    msg.objId = 0; // Assuming one temperature controller
    
    TemperatureControl data;
    data.sp_celcius = setpoint;
    data.enabled = enabled;
    // NOTE: PID values are not currently exposed to the UI, so we use defaults or existing values.
    // For now, we'll just send the setpoint and enabled status.
    // data.kp = status.temperatureControl.kp; // Example of using existing value
    // data.ki = status.temperatureControl.ki;
    // data.kd = status.temperatureControl.kd;

    msg.dataLength = sizeof(data);
    memcpy(msg.data, &data, msg.dataLength);
    
    // 2. Send IPC message
    if (ipc.sendMessage(msg)) {
        log(LOG_INFO, true, "IPC: Sent TemperatureControl update (Setpoint: %.2f, Enabled: %d)\n", setpoint, enabled);
        
        // 3. Update global status struct
        if (!statusLocked) {
            statusLocked = true;
            status.temperatureControl.sp_celcius = setpoint;
            status.temperatureControl.enabled = enabled;
            status.updated = true;
            statusLocked = false;
        }
        return true;
    } else {
        log(LOG_ERROR, true, "IPC: Failed to send TemperatureControl update.\n");
        return false;
    }
}

/**
 * @brief Updates pH control settings and sends an IPC message.
 * @param config A JsonObject containing "setpoint" and "enabled".
 * @return True on success, false on failure.
 */
bool updatePhControl(const JsonObject& config) {
    if (!config.containsKey("setpoint") || !config.containsKey("enabled")) {
        log(LOG_WARNING, true, "API: Invalid pH control payload.\n");
        return false;
    }

    float setpoint = config["setpoint"];
    bool enabled = config["enabled"];

    Message msg;
    msg.msgId = MSG_PH_CONTROL;
    msg.objId = 0;

    PHControl data;
    data.sp_pH = setpoint;
    data.enabled = enabled;
    // Other PHControl params are not set from UI yet.

    msg.dataLength = sizeof(data);
    memcpy(msg.data, &data, msg.dataLength);

    if (ipc.sendMessage(msg)) {
        log(LOG_INFO, true, "IPC: Sent PHControl update (Setpoint: %.2f, Enabled: %d)\n", setpoint, enabled);
        if (!statusLocked) {
            statusLocked = true;
            status.phControl.sp_pH = setpoint;
            status.phControl.enabled = enabled;
            status.updated = true;
            statusLocked = false;
        }
        return true;
    } else {
        log(LOG_ERROR, true, "IPC: Failed to send PHControl update.\n");
        return false;
    }
}
```

##### **3. Modify `src/utils/statusManager.h`**

Expand the `StatusVariables` struct to hold all sensor and control data.

```cpp
// src/utils/statusManager.h

// ... (keep existing includes and defines) ...

struct StatusVariables
{
    bool updated;
    // System status variables
    uint32_t ledPulseTS;
    uint32_t LEDcolour[4]; 
    bool psuOK, V20OK, V5OK, sdCardOK, ipcOK, rtcOK;

    // Component status
    bool modbusConnected, modbusBusy;
    bool webserverUp, webserverBusy;
    bool mqttConnected, mqttBusy;

    // Power supply voltages
    float Vpsu, V20, V5;

    // --- NEW: SENSOR AND CONTROL DATA ---
    // Sensor readings (from I/O Controller)
    TemperatureSensor temperatureSensor;
    PHSensor phSensor;
    DissolvedOxygenSensor doSensor;
    OpticalDensitySensor odSensor;
    // ... add all other sensor structs from IPCDataStructs.h

    // Control setpoints
    TemperatureControl temperatureControl;
    PHControl phControl;
    DissolvedOxygenControl doControl;
    // ... add all other control structs from IPCDataStructs.h
};

// ... (keep existing externs) ...
```

##### **4. Modify `src/utils/ipcManager.cpp`**

Update IPC callbacks to populate the new fields in the `StatusVariables` struct.

```cpp
// src/utils/ipcManager.cpp
#include "ipcManager.h"
#include "network/mqttManager.h"
#include "statusManager.h" // <-- Add this include

// Generic handler that forwards any valid sensor message to MQTT and updates status
void processSensorData(const Message& msg) {
    // 1. Forward to MQTT
    publishSensorData(msg);

    // 2. Update the global status struct
    if (statusLocked) return;
    statusLocked = true;

    switch (msg.msgId) {
        case MSG_TEMPERATURE_SENSOR: {
            memcpy(&status.temperatureSensor, msg.data, sizeof(TemperatureSensor));
            break;
        }
        case MSG_PH_SENSOR: {
            memcpy(&status.phSensor, msg.data, sizeof(PHSensor));
            break;
        }
        case MSG_DO_SENSOR: {
            memcpy(&status.doSensor, msg.data, sizeof(DissolvedOxygenSensor));
            break;
        }
        // ... ADD CASES FOR ALL OTHER SENSOR TYPES ...
        default:
            break; // No status update for unknown types
    }
    status.updated = true;
    statusLocked = false;
}

// ... (init_ipcManager and manageIPC remain the same) ...

void registerIpcCallbacks(void) {
    log(LOG_INFO, false, "Registering IPC callbacks for MQTT and Status...\n");

    // Register a single generic handler for all sensor types
    ipc.registerCallback(MSG_TEMPERATURE_SENSOR, processSensorData);
    ipc.registerCallback(MSG_PH_SENSOR, processSensorData);
    ipc.registerCallback(MSG_DO_SENSOR, processSensorData);
    ipc.registerCallback(MSG_OD_SENSOR, processSensorData);
    ipc.registerCallback(MSG_GAS_FLOW_SENSOR, processSensorData);
    ipc.registerCallback(MSG_PRESSURE_SENSOR, processSensorData);
    ipc.registerCallback(MSG_STIRRER_SPEED_SENSOR, processSensorData);
    ipc.registerCallback(MSG_WEIGHT_SENSOR, processSensorData);
    
    log(LOG_INFO, false, "IPC callbacks registered.\n");
}
```

##### **5. Modify `src/sys_init.h` and `sys_init.cpp`**

Integrate the new `controlManager`.

```cpp
// src/sys_init.h
// ... (in the includes section)
#include "controls/controlManager.h"

// ... (in the function prototypes section)
void init_controlManager(void);
```

```cpp
// src/sys_init.cpp
// ... (existing code) ...

void init_core1(void) {
    init_statusManager();
    init_timeManager();
    init_powerManager();
    init_terminalManager();
    init_ipcManager();
    init_controlManager(); // <-- ADD THIS
    registerIpcCallbacks();
    while (!core0setupComplete) delay(100); 
    init_sdManager();   
}

// ... (rest of file) ...
```

##### **6. Modify `src/network/network.cpp`**

Add the new API endpoints.

```cpp
// src/network/network.cpp

// At the top with other includes:
#include "../controls/controlManager.h"

// In setupWebServer(), add the new endpoint handlers:
void setupWebServer() {
    // ... (existing server.on(...) calls)

    // NEW: Comprehensive status endpoint for the UI
    server.on("/api/status/all", HTTP_GET, handleGetAllStatus);

    // NEW: Scalable control endpoint
    server.on("/api/controls", HTTP_POST, handleUpdateControl);
    
    // ... (rest of the function)
}

// Add the new handler function implementations anywhere in network.cpp
void handleGetAllStatus() {
    if (statusLocked) {
        server.send(503, "application/json", "{\"error\":\"Status temporarily unavailable\"}");
        return;
    }
    statusLocked = true;

    StaticJsonDocument<2048> doc; // Increased size for all data

    // System info
    doc["hostname"] = networkConfig.hostname;
    doc["mac"] = deviceMacAddress;

    // Internal Status
    JsonObject internal = doc.createNestedObject("internal");
    internal["psuOK"] = status.psuOK;
    internal["v20OK"] = status.V20OK;
    internal["v5OK"] = status.V5OK;
    internal["sdCardOK"] = status.sdCardOK;
    internal["ipcOK"] = status.ipcOK;
    internal["rtcOK"] = status.rtcOK;
    internal["mqttConnected"] = status.mqttConnected;

    // Sensor Readings
    JsonObject sensors = doc.createNestedObject("sensors");
    sensors["temperature"] = status.temperatureSensor.celcius;
    sensors["ph"] = status.phSensor.pH;
    sensors["do"] = status.doSensor.oxygen;
    // ... add other sensor values

    // Control Setpoints
    JsonObject controls = doc.createNestedObject("controls");
    JsonObject tempControl = controls.createNestedObject("temperature");
    tempControl["setpoint"] = status.temperatureControl.sp_celcius;
    tempControl["enabled"] = status.temperatureControl.enabled;

    JsonObject phControl = controls.createNestedObject("ph");
    phControl["setpoint"] = status.phControl.sp_pH;
    phControl["enabled"] = status.phControl.enabled;
    // ... add other control values

    statusLocked = false;

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleUpdateControl() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data received\"}");
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    const char* type = doc["type"];
    JsonObject config = doc["config"];

    if (!type || config.isNull()) {
        server.send(400, "application/json", "{\"error\":\"Invalid payload structure\"}");
        return;
    }

    bool success = false;
    if (strcmp(type, "temperature") == 0) {
        success = updateTemperatureControl(config);
    } else if (strcmp(type, "ph") == 0) {
        success = updatePhControl(config);
    }
    // ... add else-if for other control types

    if (success) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"success\":false, \"error\":\"Failed to apply control update\"}");
    }
}
```

#### **Frontend (HTML/JavaScript/CSS)**

##### **1. Replace `data/index.html`**

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Open Reactor Control System</title>
    <link rel="stylesheet" href="style.css">
</head>
<body>
    <div class="container">
        <header>
            <h1>Open Reactor Control System</h1>
            <div id="system-status"></div>
        </header>
        <nav class="tabs">
            <button class="tab-link active" onclick="openTab(event, 'dashboard')">Dashboard</button>
            <button class="tab-link" onclick="openTab(event, 'graphs')">Graphs</button>
            <button class="tab-link" onclick="openTab(event, 'settings')">Settings</button>
            <button class="tab-link" onclick="openTab(event, 'file-manager')">File Manager</button>
        </nav>

        <main id="main-content">
            <!-- Dashboard Tab -->
            <div id="dashboard" class="tab-content active">
                <h2>Live Status & Controls</h2>
                <div class="dashboard-grid">
                    <!-- Temperature Card -->
                    <div class="card">
                        <h3>Temperature</h3>
                        <p class="sensor-reading"><span id="temp-reading">--</span> &deg;C</p>
                        <div class="control-group">
                            <label for="temp-setpoint">Setpoint (&deg;C)</label>
                            <input type="number" id="temp-setpoint" step="0.1">
                            <label class="switch">
                                <input type="checkbox" id="temp-enabled">
                                <span class="slider"></span>
                            </label>
                            <button onclick="saveControl('temperature')">Save</button>
                        </div>
                    </div>
                    <!-- pH Card -->
                    <div class="card">
                        <h3>pH</h3>
                        <p class="sensor-reading"><span id="ph-reading">--</span></p>
                         <div class="control-group">
                            <label for="ph-setpoint">Setpoint</label>
                            <input type="number" id="ph-setpoint" step="0.01">
                             <label class="switch">
                                <input type="checkbox" id="ph-enabled">
                                <span class="slider"></span>
                            </label>
                            <button onclick="saveControl('ph')">Save</button>
                        </div>
                    </div>
                    <!-- Dissolved Oxygen Card -->
                     <div class="card">
                        <h3>Dissolved Oxygen</h3>
                        <p class="sensor-reading"><span id="do-reading">--</span> %</p>
                    </div>
                    <!-- Add more cards for other sensors here -->
                </div>
            </div>

            <!-- Graphs Tab -->
            <div id="graphs" class="tab-content">
                <h2>Data Graphs</h2>
                <canvas id="sensorChart"></canvas>
            </div>
            
            <!-- Settings, File Manager, etc. would be other tab-content divs -->

        </main>
    </div>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <script src="script.js"></script>
</body>
</html>
```

##### **2. Replace `data/style.css`**

```css
:root {
    --bg-color: #1a1a2e;
    --card-bg: #16213e;
    --primary-color: #0f3460;
    --accent-color: #e94560;
    --text-color: #e0e0e0;
    --font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
}
body {
    font-family: var(--font-family);
    background-color: var(--bg-color);
    color: var(--text-color);
    margin: 0;
    padding: 20px;
}
.container { max-width: 1200px; margin: 0 auto; }
header { display: flex; justify-content: space-between; align-items: center; border-bottom: 2px solid var(--primary-color); padding-bottom: 10px; }
h1 { color: var(--accent-color); }
.tabs { margin: 20px 0; }
.tab-link {
    background-color: var(--primary-color);
    color: var(--text-color);
    border: none;
    padding: 10px 20px;
    cursor: pointer;
    font-size: 1em;
    transition: background-color 0.3s;
}
.tab-link:hover, .tab-link.active { background-color: var(--accent-color); }
.tab-content { display: none; }
.tab-content.active { display: block; }
.dashboard-grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
    gap: 20px;
}
.card {
    background-color: var(--card-bg);
    padding: 20px;
    border-radius: 8px;
    box-shadow: 0 4px 8px rgba(0,0,0,0.2);
}
.card h3 { margin-top: 0; border-bottom: 1px solid var(--primary-color); padding-bottom: 10px; }
.sensor-reading { font-size: 2.5em; text-align: center; margin: 20px 0; color: var(--accent-color); }
.control-group { display: flex; flex-direction: column; gap: 10px; }
.control-group label { font-size: 0.9em; }
.control-group input[type="number"] {
    background: var(--bg-color);
    border: 1px solid var(--primary-color);
    color: var(--text-color);
    padding: 8px;
    border-radius: 4px;
    width: 95%;
}
.control-group button {
    background-color: var(--accent-color);
    color: var(--text-color);
    border: none;
    padding: 10px;
    border-radius: 4px;
    cursor: pointer;
    font-weight: bold;
    transition: transform 0.2s;
}
.control-group button:hover { transform: scale(1.05); }
/* Toggle Switch CSS */
.switch { position: relative; display: inline-block; width: 60px; height: 34px; align-self: flex-start; }
.switch input { opacity: 0; width: 0; height: 0; }
.slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 34px; }
.slider:before { position: absolute; content: ""; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; transition: .4s; border-radius: 50%; }
input:checked + .slider { background-color: var(--accent-color); }
input:checked + .slider:before { transform: translateX(26px); }
```

##### **3. Replace `data/script.js`**

```javascript
document.addEventListener('DOMContentLoaded', function() {
    fetchAndUpdateStatus();
    setInterval(fetchAndUpdateStatus, 5000); // Poll for status every 5 seconds
});

let sensorChart;

function openTab(evt, tabName) {
    let i, tabcontent, tablinks;
    tabcontent = document.getElementsByClassName("tab-content");
    for (i = 0; i < tabcontent.length; i++) {
        tabcontent[i].style.display = "none";
    }
    tablinks = document.getElementsByClassName("tab-link");
    for (i = 0; i < tablinks.length; i++) {
        tablinks[i].className = tablinks[i].className.replace(" active", "");
    }
    document.getElementById(tabName).style.display = "block";
    evt.currentTarget.className += " active";

    if (tabName === 'graphs') {
        initChart();
    }
}

async function fetchAndUpdateStatus() {
    try {
        const response = await fetch('/api/status/all');
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        const data = await response.json();
        updateDashboardUI(data);
    } catch (error) {
        console.error("Failed to fetch system status:", error);
    }
}

function updateDashboardUI(data) {
    // Update sensor readings
    document.getElementById('temp-reading').textContent = data.sensors.temperature?.toFixed(2) ?? '--';
    document.getElementById('ph-reading').textContent = data.sensors.ph?.toFixed(2) ?? '--';
    document.getElementById('do-reading').textContent = data.sensors.do?.toFixed(1) ?? '--';

    // Update control setpoints and enabled status
    if (document.activeElement.id !== 'temp-setpoint') {
        document.getElementById('temp-setpoint').value = data.controls.temperature?.setpoint?.toFixed(2) ?? '';
    }
    document.getElementById('temp-enabled').checked = data.controls.temperature?.enabled ?? false;
    
    if (document.activeElement.id !== 'ph-setpoint') {
        document.getElementById('ph-setpoint').value = data.controls.ph?.setpoint?.toFixed(2) ?? '';
    }
    document.getElementById('ph-enabled').checked = data.controls.ph?.enabled ?? false;
}

async function saveControl(controlType) {
    const setpointEl = document.getElementById(`${controlType}-setpoint`);
    const enabledEl = document.getElementById(`${controlType}-enabled`);

    const payload = {
        type: controlType,
        config: {
            setpoint: parseFloat(setpointEl.value),
            enabled: enabledEl.checked
        }
    };

    try {
        const response = await fetch('/api/controls', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(payload)
        });

        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        const result = await response.json();
        if (result.success) {
            console.log(`${controlType} control updated successfully.`);
            // Optionally show a success message to the user
        } else {
            console.error(`Failed to update ${controlType} control:`, result.error);
        }
    } catch (error) {
        console.error(`Error saving ${controlType} control:`, error);
    }
}

function initChart() {
    if (sensorChart) return; // Don't re-initialize
    const ctx = document.getElementById('sensorChart').getContext('2d');
    sensorChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [], // Timestamps will go here
            datasets: [{
                label: 'Temperature (°C)',
                data: [],
                borderColor: 'rgba(255, 99, 132, 1)',
                backgroundColor: 'rgba(255, 99, 132, 0.2)',
                fill: false,
                yAxisID: 'y-temp'
            }, {
                label: 'pH',
                data: [],
                borderColor: 'rgba(54, 162, 235, 1)',
                backgroundColor: 'rgba(54, 162, 235, 0.2)',
                fill: false,
                yAxisID: 'y-ph'
            }]
        },
        options: {
            scales: {
                x: {
                    type: 'time',
                    time: {
                        unit: 'minute'
                    },
                    title: {
                        display: true,
                        text: 'Time'
                    }
                },
                'y-temp': {
                    type: 'linear',
                    position: 'left',
                    title: {
                        display: true,
                        text: 'Temperature (°C)'
                    }
                },
                'y-ph': {
                    type: 'linear',
                    position: 'right',
                    title: {
                        display: true,
                        text: 'pH'
                    },
                    grid: {
                        drawOnChartArea: false // only draw grid for first Y axis
                    }
                }
            }
        }
    });
    // Logic to fetch historical data and update the chart would go here
}
```

---

### **3. Validation and Testing Strategy**

Follow these steps to ensure the new UI and its backing logic work correctly.

*   **Testing Steps:**
    1.  Upload the new `data` directory contents to the RP2040's LittleFS filesystem.
    2.  Build and upload the new firmware.
    3.  Open the web browser and navigate to the device's IP address.
    4.  **UI Verification:** Confirm the new tabbed layout appears, with "Dashboard" active by default.
    5.  **Graph Tab:** Click the "Graphs" tab. Verify the chart canvas appears. Click back to "Dashboard".
    6.  **Data Polling:** Open the browser's Developer Tools (F12) and go to the "Network" tab. Confirm that a request to `/api/status/all` is made every 5 seconds and returns a JSON object.
    7.  **Data Display:** Confirm that the Temperature and pH cards on the dashboard display "--" initially, then populate with values once the first API call completes.
    8.  **Setpoint Control:**
        *   In the "Temperature" card, enter `37.5` in the setpoint box and check the "Enabled" toggle.
        *   Click the "Save" button.
    9.  **API Call Verification:** In the Network tab, verify a `POST` request was sent to `/api/controls` with the payload `{"type":"temperature","config":{"setpoint":37.5,"enabled":true}}`.
    10. **Backend Logic Verification:** Open the Serial Monitor. Confirm a log message appears, such as `IPC: Sent TemperatureControl update (Setpoint: 37.50, Enabled: 1)`.

*   **Key Success Metrics:**
    *   The web UI loads correctly and presents the new tabbed interface.
    *   The dashboard cards successfully populate and periodically update with data from the `/api/status/all` endpoint.
    *   The graph is correctly isolated on the "Graphs" tab.
    *   Saving a setpoint on the dashboard triggers the correct `POST` request to the backend.
    *   The backend successfully receives the API call, processes it, and dispatches the corresponding IPC message to the I/O controller, confirmed via serial log.
    *   The UI remains responsive throughout all operations.