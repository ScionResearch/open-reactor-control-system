### **1. High-Level Analysis and Plan**

#### **Feature Interpretation**

The goal is to implement a modern, interactive web dashboard that provides a real-time overview of the system's status via WebSockets, and simultaneously allows the user to configure the system's MQTT topics. This will be achieved by:

1.  Serving a completely redesigned frontend (`index.html`, `style.css`, `app.js`).
2.  Implementing a WebSocket server on the RP2040 to broadcast a JSON status object to all connected clients periodically.
3.  Creating a persistent configuration model for MQTT topics using a JSON file on LittleFS.
4.  Exposing a REST API endpoint (`/api/mqtt/topics`) for the UI to read and write this MQTT topic configuration.
5.  Integrating a new "MQTT Topic Configuration" card into the dashboard UI.

#### **Architectural Placement & Data Flow**

This entire feature set is located on the **System Controller (RP2040)**.

*   **System Controller (RP2040) Logic:**
    *   **WebSocket Server:** A server will run on port 81. A periodic task on `Core 0` will gather system status and broadcast it as a JSON payload to all UI clients.
    *   **MQTT Topic Management:** The `mqttManager` will be responsible for loading, saving, and providing access to a `MqttTopicsConfig` struct. It will manage `/mqtt_topics.json` on the LittleFS.
    *   **Web Server:** The existing web server will handle serving the new frontend files and the new REST API endpoint for MQTT topic configuration.

*   **I/O Controller (SAMD51) & IPC Protocol (`IPCDataStructs.h`):**
    *   **No changes are required.** These components are abstracted away from the UI and MQTT implementation details.

*   **New Data Flow (Mermaid Diagram):**

    This diagram shows the parallel data flows for real-time status updates and configuration management.

    ```mermaid
    sequenceDiagram
        participant User Browser
        participant RP2040 Web Server
        participant RP2040 WebSocket Server
        participant RP2040 Core 0
        participant RP2040 (LittleFS)
        participant MQTT Broker

        User Browser->>+RP2040 Web Server: GET / (Initial Page Load)
        RP2040 Web Server-->>-User Browser: index.html, style.css, app.js

        par Real-Time Status Updates AND Configuration
            User Browser->>+RP2040 WebSocket Server: WebSocket Handshake
            RP2040 WebSocket Server-->>-User Browser: Connection Established

            loop Every 2 seconds
                RP2040 Core 0->>RP2040 WebSocket Server: Gather Status & Create JSON
                RP2040 WebSocket Server->>User Browser: broadcastTXT(jsonStatus)
            end

            User Browser->>User Browser: onmessage: Parse JSON, Update UI

        and
            User Browser->>+RP2040 Web Server: GET /api/mqtt/topics
            RP2040 Web Server-->>-User Browser: Respond with Topic JSON

            User Browser->>User Browser: User edits topics

            User Browser->>+RP2040 Web Server: POST /api/mqtt/topics (New JSON)
            RP2040 Web Server->>RP2040 (LittleFS): Save topics to file
            RP2040 Web Server-->>-User Browser: Respond {status: "success"}
        end

        Note over RP2040 Core 0, MQTT Broker: Later, MQTT publishes data using topics from LittleFS
    ```

#### **Impact Summary**

*   `platformio.ini`: **Modified** (add WebSockets library).
*   `src/network/mqttManager.h`: **Modified** (add topic config struct and prototypes).
*   `src/network/mqttManager.cpp`: **Modified** (implement topic config logic and new API).
*   `src/network/network.h`: **Modified** (add WebSocket declarations).
*   `src/network/network.cpp`: **Modified** (implement WebSocket server, call new API setup).
*   `src/sys_init.cpp`: **Modified** (add WebSocket periodic broadcast task).
*   `data/index.html`: **Replaced** (new dashboard layout with config card).
*   `data/style.css`: **Replaced** (new dashboard and form styling).
*   `data/app.js`: **Replaced** (new JS for WebSocket and API interaction).

---

### **2. Complete Implementation & Code Generation**

Here is the complete, integrated code for both features.

#### **Backend (C++)**

##### **1. Update `platformio.ini`**

```ini
; platformio.ini
...
lib_deps = 
	adafruit/Adafruit NeoPixel@^1.12.3
	bblanchon/ArduinoJson@^6.21.3
	arduino-libraries/NTPClient@^3.2.1
	greiman/SdFat@^2.3.0
	knolleary/PubSubClient@^2.8
	links2004/WebSockets@^2.4.1 ; <-- ADD THIS LINE
```

##### **2. Modify `src/network/mqttManager.h`**

```cpp
// src/network/mqttManager.h
#pragma once

#include "../sys_init.h"
#include <PubSubClient.h>

#define MQTT_TOPIC_CONFIG_FILE "/mqtt_topics.json"
#define MQTT_PUBLISH_INTERVAL 10000
#define MQTT_RECONNECT_INTERVAL 5000

// Holds all configurable MQTT topic strings.
struct MqttTopicsConfig {
    char powerVoltage[64];
};

void init_mqttManager();
void loadMqttTopicsConfig();
void saveMqttTopicsConfig();
void setupMqttApiEndpoints();
void manageMqtt();
void mqttPublishSensorData();

extern MqttTopicsConfig mqttTopicsConfig;
```

##### **3. Modify `src/network/mqttManager.cpp`**

```cpp
// src/network/mqttManager.cpp
#include "mqttManager.h"

WiFiClient ethClient; 
PubSubClient mqttClient(ethClient);
MqttTopicsConfig mqttTopicsConfig;

unsigned long lastMqttReconnectAttempt = 0;
unsigned long lastMqttPublishTime = 0;

void createDefaultMqttTopicsConfig() {
    log(LOG_INFO, true, "Creating default MQTT topic configuration...\n");
    strlcpy(mqttTopicsConfig.powerVoltage, "orcs/system/power/voltage", sizeof(mqttTopicsConfig.powerVoltage));
    saveMqttTopicsConfig();
}

void loadMqttTopicsConfig() {
    if (!LittleFS.exists(MQTT_TOPIC_CONFIG_FILE)) {
        log(LOG_WARNING, true, "MQTT topics config file not found. Creating default.\n");
        createDefaultMqttTopicsConfig();
        return;
    }
    File configFile = LittleFS.open(MQTT_TOPIC_CONFIG_FILE, "r");
    if (!configFile) {
        log(LOG_ERROR, true, "Failed to open MQTT topics config. Using defaults.\n");
        createDefaultMqttTopicsConfig();
        return;
    }
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();
    if (error) {
        log(LOG_ERROR, true, "Failed to parse MQTT topics config: %s. Using defaults.\n", error.c_str());
        createDefaultMqttTopicsConfig();
        return;
    }
    strlcpy(mqttTopicsConfig.powerVoltage, doc["powerVoltage"] | "orcs/system/power/voltage/default", sizeof(mqttTopicsConfig.powerVoltage));
    log(LOG_INFO, true, "MQTT topic configuration loaded successfully.\n");
}

void saveMqttTopicsConfig() {
    StaticJsonDocument<256> doc;
    doc["powerVoltage"] = mqttTopicsConfig.powerVoltage;
    File configFile = LittleFS.open(MQTT_TOPIC_CONFIG_FILE, "w");
    if (!configFile) {
        log(LOG_ERROR, true, "Failed to open MQTT topics config file for writing.\n");
        return;
    }
    if (serializeJson(doc, configFile) == 0) {
        log(LOG_ERROR, true, "Failed to write to MQTT topics config file.\n");
    }
    configFile.close();
}

void setupMqttApiEndpoints() {
    server.on("/api/mqtt/topics", HTTP_GET, []() {
        StaticJsonDocument<256> doc;
        doc["powerVoltage"] = mqttTopicsConfig.powerVoltage;
        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });
    server.on("/api/mqtt/topics", HTTP_POST, []() {
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
        strlcpy(mqttTopicsConfig.powerVoltage, doc["powerVoltage"] | mqttTopicsConfig.powerVoltage, sizeof(mqttTopicsConfig.powerVoltage));
        saveMqttTopicsConfig();
        server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"MQTT topics saved\"}");
    });
}

void reconnect() { /* ... function remains the same ... */ }

void publishVoltageData() {
    if (!mqttClient.connected()) return;
    StaticJsonDocument<128> doc;
    doc["value"] = status.Vpsu;
    DateTime now;
    if (getGlobalDateTime(now, 100)) {
        char timeStr[32];
        snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02dT%02d:%02d:%02dZ", now.year, now.month, now.day, now.hour, now.minute, now.second);
        doc["timestamp"] = timeStr;
    }
    char payload[128];
    serializeJson(doc, payload);
    const char* topic = mqttTopicsConfig.powerVoltage; // Use configurable topic
    if (mqttClient.publish(topic, payload)) {
        log(LOG_DEBUG, false, "MQTT Published [%s]: %s\n", topic, payload);
    } else {
        log(LOG_WARNING, true, "MQTT publish failed for topic: %s\n", topic);
    }
}

void init_mqttManager() {
    if (strlen(networkConfig.mqttBroker) > 0) {
        mqttClient.setServer(networkConfig.mqttBroker, networkConfig.mqttPort);
        loadMqttTopicsConfig(); // Load topics from LittleFS
        log(LOG_INFO, false, "MQTT Manager initialized for broker %s:%d\n", networkConfig.mqttBroker, networkConfig.mqttPort);
    } else {
        log(LOG_INFO, false, "MQTT broker not configured. MQTT Manager will remain idle.\n");
    }
}

void manageMqtt() { /* ... function remains the same ... */ }
void mqttPublishSensorData() { /* ... function remains the same ... */ }
```

##### **4. Modify `src/network/network.h`**

```cpp
// src/network/network.h
#pragma once

#include "../sys_init.h"
#include <WebSocketsServer.h> // <-- Add WebSocket include

// ... (defines remain the same) ...

void init_network(void);
void manageNetwork(void);
// ... (existing prototypes) ...

// Add new WebSocket function prototypes
void setupWebSockets(void);
void manageWebSockets(void);
void broadcastSystemStatus(void);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);

// ... (NetworkConfig struct is the same) ...

extern NetworkConfig networkConfig;
extern Wiznet5500lwIP eth;
extern WebServer server;
extern WebSocketsServer webSocket; // <-- Add WebSocket server object
extern bool ethernetConnected;
// ... (other externs) ...
```

##### **5. Modify `src/network/network.cpp`**

```cpp
// src/network/network.cpp
#include "network.h"

NetworkConfig networkConfig;
Wiznet5500lwIP eth(PIN_ETH_CS, SPI, PIN_ETH_IRQ);
WebServer server(80);
WebSocketsServer webSocket(81); // <-- Instantiate WebSocket server on port 81

// ... (other global variables) ...

void init_network() {
    setupEthernet();
    setupWebServer();
    setupWebSockets(); // <-- Add this
    // No need to call setupMqttApiEndpoints() here, it's called from setupWebServer
}

void manageNetwork(void) {
    manageEthernet();
    manageWebSockets(); // <-- Add this
    if (networkConfig.ntpEnabled) handleNTPUpdates(false);
}

void setupWebServer() {
    if (!LittleFS.begin()) { /* ... */ }
    // ... existing routes ...
    server.on("/api/system/reboot", HTTP_POST, []() { /* ... */ });
    
    // This is a good place to group the API setups
    setupNetworkAPI();
    setupMqttAPI();
    setupTimeAPI();
    setupMqttApiEndpoints(); // <-- Add call for new MQTT Topic API

    server.onNotFound([](){ handleFile(server.uri().c_str()); });
    server.begin();
    log(LOG_INFO, true, "HTTP server started\n");
    // ...
}

// ... (existing network functions) ...

// ADD ALL OF THESE NEW WEBSOCKET FUNCTIONS
void setupWebSockets() {
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    log(LOG_INFO, true, "WebSocket server started on port 81\n");
}

void manageWebSockets() {
  if (ethernetConnected) {
    webSocket.loop();
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            log(LOG_INFO, true, "[WS][%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            log(LOG_INFO, true, "[WS][%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
            broadcastSystemStatus(); // Immediately send status to new client
            break;
        }
        case WStype_TEXT:
            log(LOG_INFO, true, "[WS][%u] get text: %s\n", num, payload);
            break;
        default: break;
    }
}

void broadcastSystemStatus() {
    if (webSocket.connectedClients() == 0) return;
    StaticJsonDocument<1024> doc;
    if (!statusLocked) {
        statusLocked = true;
        doc["version"] = VERSION;
        doc["hostname"] = networkConfig.hostname;
        JsonObject power = doc.createNestedObject("power");
        power["mainVoltage"] = round(status.Vpsu * 100) / 100.0;
        power["v20Voltage"] = round(status.V20 * 100) / 100.0;
        power["v5Voltage"] = round(status.V5 * 100) / 100.0;
        power["mainVoltageOK"] = status.psuOK;
        power["v20VoltageOK"] = status.V20OK;
        power["v5VoltageOK"] = status.V5OK;
        JsonObject rtc = doc.createNestedObject("rtc");
        rtc["ok"] = status.rtcOK;
        DateTime now;
        if (getGlobalDateTime(now, 10)) {
            char timeStr[32];
            snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d", now.year, now.month, now.day, now.hour, now.minute, now.second);
            rtc["time"] = timeStr;
        } else {
            rtc["time"] = "Unknown";
        }
        JsonObject system = doc.createNestedObject("system");
        system["ipcOK"] = status.ipcOK;
        JsonObject sd = doc.createNestedObject("sd");
        if (!sdLocked) {
            sdLocked = true;
            sd["inserted"] = sdInfo.inserted;
            sd["ready"] = sdInfo.ready;
            if (sdInfo.ready) {
                sd["capacityGB"] = round(sdInfo.cardSizeBytes / 1e7) / 100.0;
                sd["freeSpaceGB"] = round(sdInfo.cardFreeBytes / 1e7) / 100.0;
                sd["logFileSizeKB"] = round(sdInfo.logSizeBytes / 10.0) / 100.0;
            }
            sdLocked = false;
        }
        JsonObject network = doc.createNestedObject("network");
        network["ethernet"] = ethernetConnected;
        network["ip"] = eth.localIP().toString();
        network["mqtt"] = status.mqttConnected;
        network["modbus"] = status.modbusConnected;
        statusLocked = false;
        String response;
        serializeJson(doc, response);
        webSocket.broadcastTXT(response);
    }
}
```

##### **6. Modify `src/sys_init.cpp`**

```cpp
// src/sys_init.cpp
// ... includes and global definitions ...

void manage_core0(void) {
    manageNetwork();
    manageMqtt();

    // Add this block for WebSocket broadcasting
    static unsigned long lastWsBroadcastTime = 0;
    const unsigned long wsBroadcastInterval = 2000; // 2 seconds

    if (millis() - lastWsBroadcastTime > wsBroadcastInterval) {
        lastWsBroadcastTime = millis();
        broadcastSystemStatus();
    }
}

void manage_core1(void) { /* ... function remains the same ... */ }
```

#### **Frontend (HTML/JavaScript/CSS)**

##### **1. `data/index.html` (Replaced)**

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Open Reactor Control System</title>
    <link rel="stylesheet" href="style.css">
    <script src="app.js" defer></script>
</head>
<body>
    <header>
        <h1>Open Reactor Control System</h1>
        <div class="connection-status">
            WebSocket: <span id="ws-status" class="status-disconnected">DISCONNECTED</span>
        </div>
    </header>
    <main class="dashboard-grid">
        <div class="card card-system">
            <h2><span class="led" id="system-led"></span>System Status</h2>
            <p><strong>Hostname:</strong> <span id="sys-hostname">...</span></p>
            <p><strong>Firmware:</strong> v<span id="sys-version">...</span></p>
            <p><strong>System Time:</strong> <span id="sys-time">...</span></p>
            <p><strong>IPC Link:</strong> <span id="status-ipc">...</span></p>
            <p><strong>RTC:</strong> <span id="status-rtc">...</span></p>
            <button id="reboot-button" class="btn-danger">Reboot System</button>
        </div>
        <div class="card card-power">
            <h2><span class="led" id="power-led"></span>Power Monitoring</h2>
            <p><strong>Main (24V):</strong> <span id="power-main-v">...</span> V <span id="status-psu"></span></p>
            <p><strong>Logic (20V):</strong> <span id="power-20v-v">...</span> V <span id="status-v20"></span></p>
            <p><strong>Periph. (5V):</strong> <span id="power-5v-v">...</span> V <span id="status-v5"></span></p>
        </div>
        <div class="card card-network">
            <h2><span class="led" id="network-led"></span>Network</h2>
            <p><strong>Ethernet:</strong> <span id="status-eth">...</span></p>
            <p><strong>IP Address:</strong> <span id="net-ip">...</span></p>
            <p><strong>Modbus:</strong> <span id="status-modbus">...</span></p>
            <p><strong>MQTT:</strong> <span id="status-mqtt">...</span></p>
        </div>
        <div class="card card-storage">
            <h2><span class="led" id="storage-led"></span>Storage (SD Card)</h2>
            <p><strong>Status:</strong> <span id="sd-status">...</span></p>
            <p><strong>Capacity:</strong> <span id="sd-capacity">...</span> GB</p>
            <p><strong>Free Space:</strong> <span id="sd-free">...</span> GB</p>
            <p><strong>Log File Size:</strong> <span id="sd-log-size">...</span> KB</p>
        </div>
        <div class="card card-mqtt-topics">
            <h2>MQTT Topic Configuration</h2>
            <div class="form-group">
                <label for="mqtt-topic-power-voltage">Power Voltage Topic</label>
                <input type="text" id="mqtt-topic-power-voltage" class="form-control" placeholder="e.g., orcs/power/voltage">
            </div>
            <button id="save-mqtt-topics-btn" class="btn-primary">Save Topics</button>
            <p id="mqtt-save-status" class="form-status-msg"></p>
        </div>
    </main>
    <footer><p>&copy; 2025 Scion Research</p></footer>
</body>
</html>
```

##### **2. `data/style.css` (Replaced)**
This combines the styles for the dashboard and the new form elements.
```css
:root {
    --bg-color: #1a1c20; --card-color: #25282e; --border-color: #3a3f4b;
    --text-color: #e0e0e0; --text-muted-color: #9e9e9e; --header-color: #2c313a;
    --accent-color: #009688; --status-ok: #4CAF50; --status-warn: #FFC107;
    --status-error: #F44336; --status-off: #607D8B;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background-color: var(--bg-color); color: var(--text-color); line-height: 1.6; }
header { background-color: var(--header-color); padding: 1rem 2rem; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid var(--border-color); }
header h1 { font-size: 1.5rem; color: var(--accent-color); }
.connection-status span { font-weight: bold; padding: 0.2rem 0.5rem; border-radius: 4px; }
.dashboard-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(350px, 1fr)); gap: 1.5rem; padding: 1.5rem; max-width: 1600px; margin: 0 auto; }
.card { background-color: var(--card-color); border: 1px solid var(--border-color); border-radius: 8px; padding: 1.5rem; box-shadow: 0 4px 12px rgba(0,0,0,0.15); }
.card h2 { display: flex; align-items: center; gap: 0.75rem; margin-bottom: 1rem; color: var(--text-color); border-bottom: 1px solid var(--border-color); padding-bottom: 0.75rem; }
.card p { margin-bottom: 0.5rem; color: var(--text-muted-color); }
.card p strong { color: var(--text-color); min-width: 120px; display: inline-block; }
.led { width: 12px; height: 12px; border-radius: 50%; display: inline-block; background-color: var(--status-off); transition: background-color 0.3s ease; }
.status-ok, .led-ok { background-color: var(--status-ok) !important; color: var(--status-ok) !important; }
.status-warn, .led-warn { background-color: var(--status-warn) !important; color: var(--status-warn) !important; }
.status-error, .led-error { background-color: var(--status-error) !important; color: var(--status-error) !important; }
.status-off, .led-off { background-color: var(--status-off) !important; color: var(--status-off) !important; }
.status-connected { background-color: var(--status-ok); color: white; }
.status-disconnected { background-color: var(--status-error); color: white; }
button { cursor: pointer; border: none; padding: 0.6rem 1.2rem; border-radius: 5px; font-weight: bold; margin-top: 1rem; transition: background-color 0.2s; }
.btn-danger { background-color: var(--status-error); color: white; }
.btn-danger:hover { background-color: #d32f2f; }
footer { text-align: center; padding: 1rem; margin-top: 2rem; color: var(--text-muted-color); font-size: 0.9rem; border-top: 1px solid var(--border-color); }
.form-group { margin-bottom: 1rem; }
.form-group label { display: block; margin-bottom: 0.25rem; color: var(--text-muted-color); font-size: 0.9rem; }
.form-control { width: 100%; padding: 0.5rem; background-color: var(--bg-color); border: 1px solid var(--border-color); color: var(--text-color); border-radius: 4px; font-size: 1rem; }
.form-control:focus { outline: none; border-color: var(--accent-color); }
.btn-primary { background-color: var(--accent-color); color: white; }
.btn-primary:hover { background-color: #00796b; }
.form-status-msg { margin-top: 1rem; font-size: 0.9rem; min-height: 1.2em; }
```

##### **3. `data/app.js` (Replaced)**

```javascript
document.addEventListener('DOMContentLoaded', () => {
    let socket;
    const wsStatusEl = document.getElementById('ws-status');
    const saveMqttTopicsBtn = document.getElementById('save-mqtt-topics-btn');
    const mqttSaveStatusEl = document.getElementById('mqtt-save-status');
    const powerVoltageTopicInput = document.getElementById('mqtt-topic-power-voltage');

    function connectWebSocket() {
        const wsUrl = `ws://${window.location.hostname}:81/`;
        socket = new WebSocket(wsUrl);
        socket.onopen = () => {
            console.log('WebSocket connected');
            wsStatusEl.textContent = 'CONNECTED';
            wsStatusEl.className = 'status-connected';
        };
        socket.onclose = () => {
            console.log('WebSocket closed. Reconnecting...');
            wsStatusEl.textContent = 'DISCONNECTED';
            wsStatusEl.className = 'status-disconnected';
            setTimeout(connectWebSocket, 3000);
        };
        socket.onerror = (error) => console.error('WebSocket error:', error);
        socket.onmessage = (event) => {
            try {
                updateUI(JSON.parse(event.data));
            } catch (e) {
                console.error('Error parsing JSON:', e);
            }
        };
    }

    function updateUI(data) {
        updateElementText('sys-hostname', data.hostname);
        updateElementText('sys-version', data.version);
        updateElementText('sys-time', data.rtc.time);
        updateStatusTextAndLed('status-ipc', 'system-led', data.system.ipcOK, 'OK', 'ERROR');
        updateStatusText('status-rtc', data.rtc.ok, 'OK', 'ERROR');
        updateElementText('power-main-v', data.power.mainVoltage.toFixed(2));
        updateStatusText('status-psu', data.power.mainVoltageOK, 'OK', 'OOR');
        updateElementText('power-20v-v', data.power.v20Voltage.toFixed(2));
        updateStatusText('status-v20', data.power.v20VoltageOK, 'OK', 'OOR');
        updateElementText('power-5v-v', data.power.v5Voltage.toFixed(2));
        updateStatusText('status-v5', data.power.v5VoltageOK, 'OK', 'OOR');
        updateLed('power-led', data.power.mainVoltageOK && data.power.v20VoltageOK && data.power.v5VoltageOK);
        updateStatusTextAndLed('status-eth', 'network-led', data.network.ethernet, 'Connected', 'Disconnected');
        updateElementText('net-ip', data.network.ip);
        updateStatusText('status-modbus', data.network.modbus, 'Connected', 'Disconnected');
        updateStatusText('status-mqtt', data.network.mqtt, 'Connected', 'Disconnected');
        if (data.sd.inserted && data.sd.ready) {
            updateStatusTextAndLed('sd-status', 'storage-led', true, 'Ready');
            updateElementText('sd-capacity', data.sd.capacityGB.toFixed(2));
            updateElementText('sd-free', data.sd.freeSpaceGB.toFixed(2));
            updateElementText('sd-log-size', data.sd.logFileSizeKB.toFixed(2));
        } else {
            updateStatusTextAndLed('sd-status', 'storage-led', false, 'Not Inserted/Ready');
            ['sd-capacity', 'sd-free', 'sd-log-size'].forEach(id => updateElementText(id, 'N/A'));
        }
        updateLed('system-led', data.system.ipcOK && data.rtc.ok && data.power.mainVoltageOK && data.sd.ready);
    }
    
    function updateElementText(id, text) { const el = document.getElementById(id); if (el && el.textContent !== text) el.textContent = text; }
    function updateStatusText(id, isOk, okText, errorText) { const el = document.getElementById(id); if(el) { el.textContent = isOk ? okText : errorText; el.className = isOk ? 'status-ok' : 'status-error'; } }
    function updateLed(id, isOk) { const el = document.getElementById(id); if (el) el.className = `led ${isOk ? 'led-ok' : 'led-error'}`; }
    function updateStatusTextAndLed(textId, ledId, isOk, okText, errorText) { updateStatusText(textId, isOk, okText, errorText); updateLed(ledId, isOk); }

    function loadMqttTopics() {
        fetch('/api/mqtt/topics').then(r => r.json()).then(d => powerVoltageTopicInput.value = d.powerVoltage || '').catch(e => console.error('Error loading MQTT topics:', e));
    }

    function saveMqttTopics() {
        mqttSaveStatusEl.textContent = 'Saving...';
        mqttSaveStatusEl.className = 'form-status-msg';
        fetch('/api/mqtt/topics', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ powerVoltage: powerVoltageTopicInput.value })
        }).then(r => r.json()).then(d => {
            if (d.status === 'success') {
                mqttSaveStatusEl.textContent = 'Topics saved successfully!';
                mqttSaveStatusEl.classList.add('status-ok');
                setTimeout(() => { mqttSaveStatusEl.textContent = ''; mqttSaveStatusEl.className = 'form-status-msg'; }, 3000);
            } else { throw new Error(d.message || 'Unknown error'); }
        }).catch(e => {
            mqttSaveStatusEl.textContent = `Error: ${e.message}`;
            mqttSaveStatusEl.classList.add('status-error');
        });
    }

    document.getElementById('reboot-button').addEventListener('click', () => { /* ... */ });
    saveMqttTopicsBtn.addEventListener('click', saveMqttTopics);

    loadMqttTopics();
    connectWebSocket();
});
```

---

### **3. Validation and Testing Strategy**

This unified test plan validates both features and their interaction.

1.  **Build and Upload:** Build the firmware and upload both the firmware and the filesystem image.

2.  **Verify UI and Real-Time Updates:**
    *   Navigate to the device's IP. The new dashboard should load.
    *   Open the browser's developer console (F12).
    *   **Success Metric:** The "WebSocket" status in the header should be "CONNECTED". The console should show a JSON message arriving every 2 seconds, and the status cards (Power, System Time, etc.) should update automatically.

3.  **Verify MQTT Topic Default Creation & Loading:**
    *   Check the serial monitor on first boot. It should log that it's creating a default `mqtt_topics.json`.
    *   In the web UI, the "MQTT Topic Configuration" card should be populated with the default topic (`orcs/system/power/voltage`).
    *   **Success Metric:** The UI correctly displays the default values loaded from the device.

4.  **Verify MQTT Topic Configuration & Persistence:**
    *   Change the topic in the input field to `lab/reactor_1/power/voltage`.
    *   Click "Save Topics". A success message should appear.
    *   Reboot the device.
    *   Refresh the web page after the device is back online.
    *   **Success Metric:** The input field should still contain `lab/reactor_1/power/voltage`, proving the setting was saved and reloaded correctly.

5.  **Verify End-to-End Functionality (The Final Test):**
    *   Open an MQTT client (like MQTT Explorer).
    *   Subscribe to the custom topic you set (`lab/reactor_1/power/voltage`).
    *   **Key Success Metric:** The MQTT client must receive voltage data on the **new, custom topic**. No data should be published to the old default topic. This confirms that the configuration is being used correctly by the `mqttManager`.