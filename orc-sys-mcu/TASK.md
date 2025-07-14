### **1. High-Level Analysis and Plan**

*   **Feature Interpretation:**
    The goal is to implement an MQTT client on the System Controller (RP2040). This client will connect to a user-configured MQTT broker and periodically publish sensor data. To prove the end-to-end data pipeline for the digital twin, we will start by publishing the main power supply voltage, which is measured locally on the RP2040. This establishes the full data path from the hardware to an external consumer like a Python script or InfluxDB.

*   **Architectural Placement & Data Flow:**
    *   **System Controller (RP2040):** All MQTT client logic, including connection management, topic generation, payload formatting (JSON), and data publishing, will reside on the System Controller. This logic will run on **Core 0**, which is dedicated to network tasks, alongside the web server and Ethernet management. A new module, `mqttManager`, will be created to encapsulate this functionality. It will read the locally-measured power supply voltage from the global `status` struct, which is updated by the `powerManager` on Core 1.
    *   **I/O Controller (SAMD51):** For this specific feature (publishing voltage), the I/O controller is not directly involved, as the power monitoring ADC pins are connected to the RP2040. However, for future sensor data (e.g., pH, Temperature), the I/O controller would be responsible for reading the sensor, packaging the data into an IPC message, and sending it to the RP2040.
    *   **IPC Protocol (`IPCDataStructs.h`):** No changes are required for this initial implementation. The existing protocol is sufficient for future expansion where data from the I/O Controller will be received and then published via MQTT.
    *   **Data Flow Diagram:**

        ```mermaid
        graph TD
            subgraph External
                User[User/Admin]
                MQTT_Broker[MQTT Broker]
                Digital_Twin[Python Script / InfluxDB]
            end

            subgraph System Controller (RP2040)
                subgraph Core 0
                    A[Web Server]
                    B[Ethernet/lwIP]
                    C[MQTT Manager]
                end
                subgraph Core 1
                    D[Power Manager]
                    E[IPC Manager]
                end
                F[Global 'status' Struct]
                G[SD Card: network_config.json]
            end
            
            subgraph I/O Controller (SAMD51)
                H[Real Sensors e.g., pH, Temp]
            end

            User -- "Configures MQTT Broker via UI" --> A
            A -- "Saves config" --> G
            C -- "Reads config" --> G
            
            D -- "Measures Voltage" --> F
            C -- "Reads status.Vpsu" --> F
            C -- "Publishes Data" --> B
            B -- "TCP/IP" --> MQTT_Broker
            
            MQTT_Broker -- "Forwards Data" --> Digital_Twin
            
            subgraph Future Expansion
                H -- "Sensor Data" --> E
                E -- "Updates Global State via IPC" --> F
            end

            style C fill:#f9f,stroke:#333,stroke-width:2px
        ```

*   **Impact Summary:**
    *   `platformio.ini`: Add `PubSubClient` library dependency.
    *   `src/sys_init.h`: Include the new `mqttManager.h`.
    *   `src/sys_init.cpp`: Add calls to initialize and manage the MQTT client on Core 0.
    *   `src/network/network.cpp`: Update `status.mqttConnected` based on connection state.
    *   `src/utils/statusManager.h`: The `status` struct already contains `mqttConnected` and `mqttBusy` flags, which is perfect. No changes needed.
    *   **New File:** `src/network/mqttManager.h`: Header for the new MQTT module.
    *   **New File:** `src/network/mqttManager.cpp`: Implementation of the MQTT client logic.

---

### **2. Complete Implementation & Code Generation**

Here are all the necessary code modifications and new files.

*   **Backend (C++):**

    **1. Modify `platformio.ini`**

    Add the `PubSubClient` library to your dependencies.

    *File Path:* `platformio.ini`

    ```ini
    ; ... (existing content) ...
    lib_deps = 
        adafruit/Adafruit NeoPixel@^1.12.3
        bblanchon/ArduinoJson@^6.21.3
        arduino-libraries/NTPClient@^3.2.1
        greiman/SdFat@^2.3.0
        knolleary/PubSubClient@^2.8
    ```

    **2. Create New File `src/network/mqttManager.h`**

    This header file defines the public interface for our new MQTT manager.

    *File Path:* `src/network/mqttManager.h`

    ```cpp
    #pragma once

    #include "../sys_init.h"
    #include <PubSubClient.h>

    #define MQTT_PUBLISH_INTERVAL 10000 // Publish data every 10 seconds
    #define MQTT_RECONNECT_INTERVAL 5000 // Attempt to reconnect every 5 seconds

    /**
     * @brief Initializes the MQTT client with server details from config.
     */
    void init_mqttManager();

    /**
     * @brief Manages the MQTT connection and periodic data publishing.
     *        Should be called repeatedly in the network loop (Core 0).
     */
    void manageMqtt();

    /**
     * @brief Publishes sensor data to the MQTT broker.
     */
    void mqttPublishSensorData();

    ```

    **3. Create New File `src/network/mqttManager.cpp`**

    This is the core implementation of the MQTT client.

    *File Path:* `src/network/mqttManager.cpp`

    ```cpp
    #include "mqttManager.h"

    // Use the base WiFiClient for PubSubClient, even with Ethernet
    WiFiClient ethClient; 
    PubSubClient mqttClient(ethClient);

    unsigned long lastMqttReconnectAttempt = 0;
    unsigned long lastMqttPublishTime = 0;

    /**
     * @brief Attempts to reconnect to the MQTT broker.
     */
    void reconnect() {
        if (strlen(networkConfig.mqttBroker) == 0) {
            return; // Don't attempt to connect if broker is not configured
        }

        log(LOG_INFO, true, "Attempting MQTT connection to %s...", networkConfig.mqttBroker);
        
        // Create a unique client ID from the MAC address
        char clientId[24];
        uint8_t mac[6];
        eth.macAddress(mac);
        snprintf(clientId, sizeof(clientId), "ORCS-%02X%02X%02X", mac[3], mac[4], mac[5]);

        // Attempt to connect
        bool connected = false;
        if (strlen(networkConfig.mqttUsername) > 0) {
            connected = mqttClient.connect(clientId, networkConfig.mqttUsername, networkConfig.mqttPassword);
        } else {
            connected = mqttClient.connect(clientId);
        }

        if (connected) {
            log(LOG_INFO, true, "MQTT connected successfully!\n");
            // TODO: Add subscriptions for commands here in the future
            // mqttClient.subscribe("orcs/system/command");
        } else {
            log(LOG_WARNING, true, "MQTT connection failed, rc=%d. Will retry in %d seconds.\n", mqttClient.state(), MQTT_RECONNECT_INTERVAL / 1000);
        }
    }

    /**
     * @brief Publishes the main PSU voltage to the MQTT broker.
     */
    void publishVoltageData() {
        if (!mqttClient.connected()) {
            return;
        }

        // Create JSON payload
        StaticJsonDocument<128> doc;
        doc["value"] = status.Vpsu;

        // Add timestamp
        DateTime now;
        if (getGlobalDateTime(now, 100)) {
            char timeStr[32];
            snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02dT%02d:%02d:%02dZ", 
                     now.year, now.month, now.day, now.hour, now.minute, now.second);
            doc["timestamp"] = timeStr;
        }

        char payload[128];
        serializeJson(doc, payload);

        // Define topic
        const char* topic = "orcs/system/power/voltage";

        // Publish
        if (mqttClient.publish(topic, payload)) {
            log(LOG_DEBUG, false, "MQTT Published [%s]: %s\n", topic, payload);
        } else {
            log(LOG_WARNING, true, "MQTT publish failed for topic: %s\n", topic);
        }
    }

    void init_mqttManager() {
        if (strlen(networkConfig.mqttBroker) > 0) {
            mqttClient.setServer(networkConfig.mqttBroker, networkConfig.mqttPort);
            log(LOG_INFO, false, "MQTT Manager initialized for broker %s:%d\n", networkConfig.mqttBroker, networkConfig.mqttPort);
        } else {
            log(LOG_INFO, false, "MQTT broker not configured. MQTT Manager will remain idle.\n");
        }
    }

    void manageMqtt() {
        if (!ethernetConnected || strlen(networkConfig.mqttBroker) == 0) {
            if (status.mqttConnected) {
                if (!statusLocked) {
                    statusLocked = true;
                    status.mqttConnected = false;
                    status.updated = true;
                    statusLocked = false;
                }
            }
            return;
        }

        if (!mqttClient.connected()) {
            if (status.mqttConnected) { // Update status if we just disconnected
                if (!statusLocked) {
                    statusLocked = true;
                    status.mqttConnected = false;
                    status.updated = true;
                    statusLocked = false;
                }
            }
            // Check if it's time to try reconnecting
            if (millis() - lastMqttReconnectAttempt > MQTT_RECONNECT_INTERVAL) {
                lastMqttReconnectAttempt = millis();
                reconnect();
            }
        } else {
             if (!status.mqttConnected) { // Update status if we just connected
                if (!statusLocked) {
                    statusLocked = true;
                    status.mqttConnected = true;
                    status.updated = true;
                    statusLocked = false;
                }
            }
            // Process MQTT messages and keep-alives
            mqttClient.loop();

            // Check if it's time to publish data
            if (millis() - lastMqttPublishTime > MQTT_PUBLISH_INTERVAL) {
                lastMqttPublishTime = millis();
                publishVoltageData();
            }
        }
    }
    ```

    **4. Modify `src/sys_init.h`**

    Include the new MQTT manager header.

    *File Path:* `src/sys_init.h`

    ```cpp
    // ... (around line 18)
    // Include program files
    #include "hardware/pins.h"

    #include "network/network.h"
    #include "network/mqttManager.h" // <-- ADD THIS LINE

    #include "utils/logger.h"
    // ... (rest of the file)
    ```

    **5. Modify `src/sys_init.cpp`**

    Integrate the MQTT manager into the Core 0 initialization and management loop.

    *File Path:* `src/sys_init.cpp`

    ```cpp
    // ... (around line 15)
    void init_core0(void) {
        init_logger();
        init_network();
        init_mqttManager(); // <-- ADD THIS LINE
    }

    void init_core1(void) {
    // ... (no changes here)
    }

    void manage_core0(void) {
        manageNetwork();
        manageMqtt(); // <-- ADD THIS LINE
    }

    void manage_core1(void) {
    // ... (no changes here)
    }
    ```

*   **Frontend (HTML/JavaScript/CSS):**
    No frontend changes are required by this feature request, as the existing MQTT configuration API (`/api/mqtt`) is already in place and can be used to set the broker details.

*   **API & Communication:**

    *   **REST API Endpoints:** No new endpoints are needed. The existing `POST /api/mqtt` is used for configuration.

    *   **MQTT Topics and Payload Structures:**
        *   **Topic:** `orcs/system/power/voltage`
        *   **Payload Structure (JSON):**
            ```json
            {
              "value": 24.15,
              "timestamp": "2025-07-15T10:30:00Z"
            }
            ```
            *   `value`: The measured floating-point voltage.
            *   `timestamp`: An ISO 8601 formatted string of the time the measurement was published.

*   **Configuration & Storage:**
    The system already stores MQTT settings (`mqttBroker`, `mqttPort`, etc.) in `network_config.json` on the LittleFS partition. This feature leverages that existing mechanism. No changes are required.

---

### **3. Validation and Testing Strategy**

*   **Testing Steps:**

    1.  **Compile and Upload:** Build and upload the modified firmware to the System Controller (RP2040).
    2.  **Configure MQTT Broker:**
        *   Navigate to the web UI in your browser.
        *   Go to the "Network Settings" page.
        *   In the "MQTT Configuration" section, enter your MQTT broker's IP address or hostname, port (usually 1883), and credentials (if any).
        *   Click "Save" and reboot the device when prompted.
    3.  **Verify Connection:**
        *   Open the serial monitor.
        *   After the device boots and connects to the network, you should see logs like:
            `[INFO] MQTT Manager initialized for broker 192.168.1.50:1883`
            `[INFO] Attempting MQTT connection to 192.168.1.50...`
            `[INFO] MQTT connected successfully!`
        *   Check the status LEDs on the device. The MQTT status LED should turn solid green.
    4.  **Subscribe to Topic:**
        *   Use an MQTT client (like MQTT Explorer, or `mosquitto_sub` on the command line).
        *   Connect the client to your MQTT broker.
        *   Subscribe to the topic: `orcs/system/power/voltage`
    5.  **Confirm Data Publication:**
        *   Observe the MQTT client. You should see a new message appear on the topic every 10 seconds.
        *   Verify the payload of the message is valid JSON and matches the specified structure, containing a `value` and a `timestamp`.
        *   Check the serial monitor for debug logs like: `[DEBUG] MQTT Published [orcs/system/power/voltage]: {"value":24.1,"timestamp":"..."}`

*   **Key Success Metrics:**
    *   **Successful Connection:** The device successfully establishes and maintains a connection to the MQTT broker, confirmed by serial logs and the green status LED.
    *   **Periodic Data Publication:** Valid JSON data containing the PSU voltage and a timestamp is published to the `orcs/system/power/voltage` topic every 10 seconds.
    *   **Data Integrity:** The data received by the external MQTT client is well-formed and accurately reflects the voltage being measured by the device.
    *   **System Stability:** The device remains stable and responsive (web UI, etc.) while the MQTT client is running.