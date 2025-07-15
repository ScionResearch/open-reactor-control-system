### **1. High-Level Analysis and Plan**

#### **Feature Interpretation**

The goal is to transform the current, likely static and basic, web UI into a modern, interactive dashboard. This involves a complete redesign of the frontend (HTML, CSS, JavaScript) to provide a clear, at-a-glance overview of the system's status. The "interactive" aspect will be achieved by implementing real-time data updates using WebSockets, eliminating the need for page refreshes. The new UI will be visually appealing, organized into logical "cards" for each subsystem, and will provide a professional and intuitive user experience.

#### **Architectural Placement & Data Flow**

This feature primarily impacts the **System Controller (RP2040)**, as it is responsible for all network and UI-related tasks.

*   **System Controller (RP2040) Logic:**
    *   Serve new, completely redesigned `index.html`, `style.css`, and `app.js` files from LittleFS.
    *   Implement a **WebSocket server** alongside the existing HTTP server.
    *   Create a periodic task (in `manage_core0`) that gathers the system's status from the global `status` struct and other managers.
    *   Serialize this status into a single JSON object.
    *   Broadcast this JSON object to all connected WebSocket clients every 2 seconds.

*   **I/O Controller (SAMD51) Logic:**
    *   **No changes are required on the I/O Controller.** Its responsibility remains to sense, control, and report its state to the System Controller via the existing IPC protocol. The System Controller will then use this data to update the UI.

*   **IPC Protocol (`IPCDataStructs.h`):**
    *   **No changes are required.** The existing data structures are sufficient for the System Controller to build the status overview.

*   **New Data Flow (Mermaid Diagram):**

    The new flow adds a persistent WebSocket connection for real-time updates after the initial page load.

    ```mermaid
    sequenceDiagram
        participant User Browser
        participant RP2040 Web Server
        participant RP2040 WebSocket Server
        participant RP2040 Core 0 Loop

        User Browser->>+RP2040 Web Server: GET / (Request page)
        RP2040 Web Server-->>-User Browser: Responds with index.html, style.css, app.js

        User Browser->>+RP2040 WebSocket Server: WebSocket Handshake (Upgrade Request)
        RP2040 WebSocket Server-->>-User Browser: Handshake Successful (Connection established)
        Note over User Browser: UI shows "Connecting..."

        loop Every 2 seconds
            RP2040 Core 0 Loop->>RP2040 Core 0 Loop: 1. Gather status (Power, SD, Time, etc.)
            RP2040 Core 0 Loop->>RP2040 WebSocket Server: 2. Create JSON status object
            RP2040 WebSocket Server->>User Browser: 3. broadcastTXT(jsonStatus)
        end

        User Browser->>User Browser: onmessage: Parse JSON and update DOM
    ```

#### **Impact Summary**

The following files will be modified or added:

*   `platformio.ini`: Add WebSocket library dependency.
*   `src/sys_init.cpp`: Add initialization and management calls for WebSockets.
*   `src/network/network.h`: Add declarations for WebSocket server object and functions.
*   `src/network/network.cpp`: Add WebSocket server implementation, event handling, and the periodic status broadcast logic.
*   `data/index.html` **(Replaced)**: New dashboard structure.
*   `data/style.css` **(New)**: New CSS for modern styling.
*   `data/app.js` **(New)**: New JavaScript for WebSocket communication and dynamic UI updates.

---

### **2. Complete Implementation & Code Generation**

Here are all the necessary code and configuration changes.

#### **Backend (C++)**

##### **1. Update `platformio.ini`**

Add the `links2004/WebSockets` library to your dependencies.

```ini
; platformio.ini

[env:pico]
platform = https://github.com/maxgerhardt/platform-raspberrypi.git
board = generic
framework = arduino
board_build.core = earlephilhower
board_build.filesystem_size = 256k
board_build.f_cpu = 200000000L
board_build.arduino.earlephilhower.usb_manufacturer = Scion Research
board_build.arduino.earlephilhower.usb_product = Open Bio-reactor Control System
board_build.arduino.earlephilhower.usb_vid = 0x04D8
board_build.arduino.earlephilhower.usb_pid = 0xEB63
lib_deps = 
	adafruit/Adafruit NeoPixel@^1.12.3
	bblanchon/ArduinoJson@^6.21.3
	arduino-libraries/NTPClient@^3.2.1
	greiman/SdFat@^2.3.0
	links2004/WebSockets@^2.4.1 ; <-- ADD THIS LINE
```

##### **2. Modify `src/network/network.h`**

Include the WebSockets header and declare the new objects and functions.

```cpp
// src/network/network.h

#pragma once

#include "../sys_init.h"
#include <WebSocketsServer.h> // <-- ADD THIS LINE

// LittleFS configuration
// ... (rest of the file is the same until extern declarations)

// Add new function prototypes
void setupWebSockets(void);
void manageWebSockets(void);
void broadcastSystemStatus(void);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);

// ... (rest of NetworkConfig struct is the same)

// Global variables
extern NetworkConfig networkConfig;

extern Wiznet5500lwIP eth;
extern WebServer server;
extern WebSocketsServer webSocket; // <-- ADD THIS LINE

// ... (rest of the file is the same)
```

##### **3. Modify `src/network/network.cpp`**

Instantiate the WebSocket server and implement the new logic.

```cpp
// src/network/network.cpp

#include "network.h"

// Global variables
NetworkConfig networkConfig;

Wiznet5500lwIP eth(PIN_ETH_CS, SPI, PIN_ETH_IRQ);
WebServer server(80);
WebSocketsServer webSocket(81); // <-- ADD THIS LINE on port 81

// ... (rest of global variables)

// Network component initialisation functions ------------------------------>
void init_network() {
    setupEthernet();
    setupWebServer();
    setupWebSockets(); // <-- ADD THIS LINE
    setupNetworkAPI();
    setupMqttAPI();
    setupTimeAPI();
}

void manageNetwork(void) {
    manageEthernet();
    manageWebSockets(); // <-- ADD THIS LINE
    if (networkConfig.ntpEnabled) handleNTPUpdates(false);
}

// ... (existing functions: setupEthernet, loadNetworkConfig, saveNetworkConfig, applyNetworkConfig, setupNetworkAPI)

// ADD THE FOLLOWING NEW FUNCTIONS:

void setupWebSockets() {
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    log(LOG_INFO, true, "WebSocket server started on port 81\n");
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            log(LOG_INFO, true, "[WS][%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                log(LOG_INFO, true, "[WS][%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
                // Immediately send current status to the new client
                broadcastSystemStatus();
            }
            break;
        case WStype_TEXT:
            log(LOG_INFO, true, "[WS][%u] get text: %s\n", num, payload);
            // We don't expect any messages from client in this implementation
            break;
        case WStype_BIN:
        case WStype_ERROR:
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
            break;
    }
}

void broadcastSystemStatus() {
    if (webSocket.connectedClients() == 0) {
        return; // No one is listening
    }

    StaticJsonDocument<1024> doc; // Increased size to be safe

    // This logic is copied and adapted from the /api/system/status endpoint
    if (!statusLocked) {
        statusLocked = true;
        
        // System Meta
        doc["version"] = VERSION;
        doc["hostname"] = networkConfig.hostname;

        // Power supplies
        JsonObject power = doc.createNestedObject("power");
        power["mainVoltage"] = round(status.Vpsu * 100) / 100.0;
        power["v20Voltage"] = round(status.V20 * 100) / 100.0;
        power["v5Voltage"] = round(status.V5 * 100) / 100.0;
        power["mainVoltageOK"] = status.psuOK;
        power["v20VoltageOK"] = status.V20OK;
        power["v5VoltageOK"] = status.V5OK;
        
        // RTC status
        JsonObject rtc = doc.createNestedObject("rtc");
        rtc["ok"] = status.rtcOK;
        
        DateTime now;
        if (getGlobalDateTime(now, 10)) {
            char timeStr[32];
            snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d", 
                     now.year, now.month, now.day, now.hour, now.minute, now.second);
            rtc["time"] = timeStr;
        } else {
            rtc["time"] = "Unknown";
        }
        
        // Core System Status
        JsonObject system = doc.createNestedObject("system");
        system["ipcOK"] = status.ipcOK;

        // SD card info
        JsonObject sd = doc.createNestedObject("sd");
        if (!sdLocked) {
            sdLocked = true;
            sd["inserted"] = sdInfo.inserted;
            sd["ready"] = sdInfo.ready;
            
            if (sdInfo.ready) {
                sd["capacityGB"] = round(sdInfo.cardSizeBytes / 1e7) / 100.0; // Round to 2 decimal places
                sd["freeSpaceGB"] = round(sdInfo.cardFreeBytes / 1e7) / 100.0;
                sd["logFileSizeKB"] = round(sdInfo.logSizeBytes / 10.0) / 100.0;
                sd["sensorFileSizeKB"] = round(sdInfo.sensorSizeBytes / 10.0) / 100.0;
            }
            sdLocked = false;
        }
        
        // Network connections
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

// ... (rest of the file, e.g., setupWebServer, handleWebServer etc.)

// Add this function call to handleWebServer
void handleWebServer() {
  if(!ethernetConnected) {
    return;
  }
  server.handleClient();
  // webSocket.loop(); // Moved to manageWebSockets
  // ... (rest of function)
}

// ADD THIS NEW FUNCTION
void manageWebSockets() {
  if (ethernetConnected) {
    webSocket.loop();
  }
}

```

##### **4. Modify `src/sys_init.cpp`**

Add the periodic broadcast call to the main loop for Core 0.

```cpp
// src/sys_init.cpp

#include "sys_init.h"

// Object definitions
IPCProtocol ipc(Serial1);
// ... (rest of definitions)

// ... (init functions are the same)

void manage_core0(void) {
    manageNetwork();

    // Add this block for WebSocket broadcasting
    static unsigned long lastWsBroadcastTime = 0;
    const unsigned long wsBroadcastInterval = 2000; // 2 seconds

    if (millis() - lastWsBroadcastTime > wsBroadcastInterval) {
        lastWsBroadcastTime = millis();
        broadcastSystemStatus();
    }
}

void manage_core1(void) {
    manageStatus();
    // ... (rest of function is the same)
}

```

---

#### **Frontend (HTML/JavaScript/CSS)**

Create these new files in your project's `data/` directory. **You must upload these files to the RP2040's LittleFS filesystem** for them to be served.

##### **1. `data/index.html` (New File)**

This file provides the structure for the new dashboard.

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
            <p><strong>Hostname:</strong> <span id="sys-hostname">Loading...</span></p>
            <p><strong>Firmware:</strong> v<span id="sys-version">...</span></p>
            <p><strong>System Time:</strong> <span id="sys-time">Loading...</span></p>
            <p><strong>IPC Link:</strong> <span id="status-ipc">Loading...</span></p>
            <p><strong>RTC:</strong> <span id="status-rtc">Loading...</span></p>
            <button id="reboot-button" class="btn-danger">Reboot System</button>
        </div>

        <div class="card card-power">
            <h2><span class="led" id="power-led"></span>Power Monitoring</h2>
            <p><strong>Main Supply (24V):</strong> <span id="power-main-v">...</span> V <span id="status-psu"></span></p>
            <p><strong>Control Logic (20V):</strong> <span id="power-20v-v">...</span> V <span id="status-v20"></span></p>
            <p><strong>Peripheral (5V):</strong> <span id="power-5v-v">...</span> V <span id="status-v5"></span></p>
        </div>

        <div class="card card-network">
            <h2><span class="led" id="network-led"></span>Network</h2>
            <p><strong>Ethernet:</strong> <span id="status-eth">Loading...</span></p>
            <p><strong>IP Address:</strong> <span id="net-ip">...</span></p>
            <p><strong>Modbus:</strong> <span id="status-modbus">Loading...</span></p>
            <p><strong>MQTT:</strong> <span id="status-mqtt">Loading...</span></p>
        </div>

        <div class="card card-storage">
            <h2><span class="led" id="storage-led"></span>Storage (SD Card)</h2>
            <p><strong>Status:</strong> <span id="sd-status">Loading...</span></p>
            <p><strong>Capacity:</strong> <span id="sd-capacity">...</span> GB</p>
            <p><strong>Free Space:</strong> <span id="sd-free">...</span> GB</p>
            <p><strong>Log File Size:</strong> <span id="sd-log-size">...</span> KB</p>
        </div>
    </main>

    <footer>
        <p>&copy; 2025 Scion Research - Open Reactor Control System</p>
    </footer>
</body>
</html>
```

##### **2. `data/style.css` (New File)**

This file provides the modern, dark-theme styling.

```css
:root {
    --bg-color: #1a1c20;
    --card-color: #25282e;
    --border-color: #3a3f4b;
    --text-color: #e0e0e0;
    --text-muted-color: #9e9e9e;
    --header-color: #2c313a;
    --accent-color: #009688;
    --status-ok: #4CAF50;
    --status-warn: #FFC107;
    --status-error: #F44336;
    --status-off: #607D8B;
}

* {
    box-sizing: border-box;
    margin: 0;
    padding: 0;
}

body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    background-color: var(--bg-color);
    color: var(--text-color);
    line-height: 1.6;
}

header {
    background-color: var(--header-color);
    padding: 1rem 2rem;
    display: flex;
    justify-content: space-between;
    align-items: center;
    border-bottom: 1px solid var(--border-color);
}

header h1 {
    font-size: 1.5rem;
    color: var(--accent-color);
}

.connection-status span {
    font-weight: bold;
    padding: 0.2rem 0.5rem;
    border-radius: 4px;
}

.dashboard-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));
    gap: 1.5rem;
    padding: 1.5rem;
    max-width: 1600px;
    margin: 0 auto;
}

.card {
    background-color: var(--card-color);
    border: 1px solid var(--border-color);
    border-radius: 8px;
    padding: 1.5rem;
    box-shadow: 0 4px 12px rgba(0,0,0,0.15);
}

.card h2 {
    display: flex;
    align-items: center;
    gap: 0.75rem;
    margin-bottom: 1rem;
    color: var(--text-color);
    border-bottom: 1px solid var(--border-color);
    padding-bottom: 0.75rem;
}

.card p {
    margin-bottom: 0.5rem;
    color: var(--text-muted-color);
}

.card p strong {
    color: var(--text-color);
    min-width: 120px;
    display: inline-block;
}

.card p span {
    color: var(--text-color);
}

.led {
    width: 12px;
    height: 12px;
    border-radius: 50%;
    display: inline-block;
    background-color: var(--status-off);
    transition: background-color 0.3s ease;
}

.status-ok, .led-ok { background-color: var(--status-ok) !important; color: var(--status-ok) !important; }
.status-warn, .led-warn { background-color: var(--status-warn) !important; color: var(--status-warn) !important; }
.status-error, .led-error { background-color: var(--status-error) !important; color: var(--status-error) !important; }
.status-off, .led-off { background-color: var(--status-off) !important; color: var(--status-off) !important; }

.status-connected { background-color: var(--status-ok); color: white; }
.status-disconnected { background-color: var(--status-error); color: white; }

button {
    cursor: pointer;
    border: none;
    padding: 0.6rem 1.2rem;
    border-radius: 5px;
    font-weight: bold;
    margin-top: 1rem;
    transition: background-color 0.2s;
}

.btn-danger {
    background-color: var(--status-error);
    color: white;
}

.btn-danger:hover {
    background-color: #d32f2f;
}

footer {
    text-align: center;
    padding: 1rem;
    margin-top: 2rem;
    color: var(--text-muted-color);
    font-size: 0.9rem;
    border-top: 1px solid var(--border-color);
}
```

##### **3. `data/app.js` (New File)**

This file handles all the client-side logic for WebSocket communication and DOM manipulation.

```javascript
document.addEventListener('DOMContentLoaded', () => {
    const wsStatusEl = document.getElementById('ws-status');
    let socket;

    function connectWebSocket() {
        // Use the page's hostname to connect, on port 81 for WS
        const wsUrl = `ws://${window.location.hostname}:81/`;
        socket = new WebSocket(wsUrl);

        socket.onopen = () => {
            console.log('WebSocket connection established');
            wsStatusEl.textContent = 'CONNECTED';
            wsStatusEl.className = 'status-connected';
        };

        socket.onclose = () => {
            console.log('WebSocket connection closed. Reconnecting in 3 seconds...');
            wsStatusEl.textContent = 'DISCONNECTED';
            wsStatusEl.className = 'status-disconnected';
            setTimeout(connectWebSocket, 3000);
        };

        socket.onerror = (error) => {
            console.error('WebSocket error:', error);
            wsStatusEl.textContent = 'ERROR';
            wsStatusEl.className = 'status-disconnected';
        };

        socket.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                updateUI(data);
            } catch (e) {
                console.error('Error parsing JSON from WebSocket:', e);
            }
        };
    }

    function updateUI(data) {
        // System Status Card
        updateElementText('sys-hostname', data.hostname);
        updateElementText('sys-version', data.version);
        updateElementText('sys-time', data.rtc.time);
        updateStatusTextAndLed('status-ipc', 'system-led', data.system.ipcOK, 'OK', 'ERROR');
        updateStatusText('status-rtc', data.rtc.ok, 'OK', 'ERROR');

        // Power Card
        updateElementText('power-main-v', data.power.mainVoltage.toFixed(2));
        updateStatusText('status-psu', data.power.mainVoltageOK, 'OK', 'OOR');
        updateElementText('power-20v-v', data.power.v20Voltage.toFixed(2));
        updateStatusText('status-v20', data.power.v20VoltageOK, 'OK', 'OOR');
        updateElementText('power-5v-v', data.power.v5Voltage.toFixed(2));
        updateStatusText('status-v5', data.power.v5VoltageOK, 'OK', 'OOR');
        
        const isPowerOk = data.power.mainVoltageOK && data.power.v20VoltageOK && data.power.v5VoltageOK;
        updateLed('power-led', isPowerOk);

        // Network Card
        updateStatusTextAndLed('status-eth', 'network-led', data.network.ethernet, 'Connected', 'Disconnected');
        updateElementText('net-ip', data.network.ip);
        updateStatusText('status-modbus', data.network.modbus, 'Connected', 'Disconnected');
        updateStatusText('status-mqtt', data.network.mqtt, 'Connected', 'Disconnected');

        // Storage Card
        if (data.sd.inserted && data.sd.ready) {
            updateStatusTextAndLed('sd-status', 'storage-led', true, 'Ready', 'Error');
            updateElementText('sd-capacity', data.sd.capacityGB.toFixed(2));
            updateElementText('sd-free', data.sd.freeSpaceGB.toFixed(2));
            updateElementText('sd-log-size', data.sd.logFileSizeKB.toFixed(2));
        } else {
            updateStatusTextAndLed('sd-status', 'storage-led', false, 'Not Inserted/Ready', 'Error');
            updateElementText('sd-capacity', 'N/A');
            updateElementText('sd-free', 'N/A');
            updateElementText('sd-log-size', 'N/A');
        }

        // Update main system LED based on overall health
        const isSystemOk = data.system.ipcOK && data.rtc.ok && isPowerOk && data.sd.ready;
        updateLed('system-led', isSystemOk);
    }
    
    // Helper functions for updating the DOM
    function updateElementText(id, text) {
        const el = document.getElementById(id);
        if (el && el.textContent !== text) {
            el.textContent = text;
        }
    }

    function updateStatusText(id, isOk, okText = 'OK', errorText = 'ERROR') {
        const el = document.getElementById(id);
        if (el) {
            el.textContent = isOk ? okText : errorText;
            el.className = isOk ? 'status-ok' : 'status-error';
        }
    }
    
    function updateLed(id, isOk) {
        const el = document.getElementById(id);
        if (el) {
            el.className = `led ${isOk ? 'led-ok' : 'led-error'}`;
        }
    }

    function updateStatusTextAndLed(textId, ledId, isOk, okText, errorText) {
        updateStatusText(textId, isOk, okText, errorText);
        updateLed(ledId, isOk);
    }

    // Reboot Button
    document.getElementById('reboot-button').addEventListener('click', () => {
        if (confirm('Are you sure you want to reboot the system?')) {
            fetch('/api/system/reboot', { method: 'POST' })
                .then(response => response.json())
                .then(data => {
                    console.log('Reboot command sent:', data);
                    alert('System is rebooting. This page will disconnect and attempt to reconnect automatically.');
                })
                .catch(error => console.error('Error sending reboot command:', error));
        }
    });

    // Initial connection
    connectWebSocket();
});
```

---

### **3. Validation and Testing Strategy**

Follow these steps to verify the new feature works correctly.

1.  **Build and Upload Filesystem:**
    *   Place the new `index.html`, `style.css`, and `app.js` files in the `data` directory of your PlatformIO project.
    *   Use the "Build Filesystem Image" and "Upload Filesystem Image" tasks in PlatformIO to write the new UI to the RP2040's LittleFS.
    *   Build and upload the main firmware with the C++ changes.

2.  **Load the UI:**
    *   Open a modern web browser (like Chrome or Firefox) and navigate to the device's IP address (e.g., `http://192.168.1.100`).
    *   **Success Metric:** The new dark-themed dashboard should load, displaying the various cards with "Loading..." or "..." placeholders.

3.  **Verify WebSocket Connection:**
    *   Open the browser's developer tools (F12) and go to the "Network" tab. Filter for "WS" (WebSockets).
    *   **Success Metric:** You should see a connection to `ws://<ip-address>:81` with a status of `101 Switching Protocols`. In the "Console" tab, you should see the "WebSocket connection established" message. The status indicator in the UI header should turn green and say "CONNECTED".

4.  **Test Real-Time Updates:**
    *   Keep the developer tools open on the "Network" -> "WS" tab and click on the connection name.
    *   **Success Metric:** You should see a new JSON message arriving from the server every 2 seconds. The values on the dashboard (System Time, voltages, etc.) should update in real-time without the page refreshing.

5.  **Test Status Logic:**
    *   While the system is running, physically remove the SD card.
    *   **Success Metric:** Within a few seconds, the "Storage" card should update to show "Status: Not Inserted/Ready", its LED should turn red, and the main "System Status" LED should also turn red, indicating an overall system health issue. Re-insert the card, and the status should return to "Ready" and green.

6.  **Test Interactive Controls:**
    *   Click the "Reboot System" button. A browser confirmation dialog should appear.
    *   Click "OK".
    *   **Success Metric:** The device's LEDs should restart their sequence. The WebSocket connection in the browser will close. After the device boots up and connects to the network, the JavaScript will automatically re-establish the WebSocket connection, and the UI will resume receiving live data.