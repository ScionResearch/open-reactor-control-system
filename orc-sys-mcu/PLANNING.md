# Open Reactor Control System - MCU Architecture Overview

This document provides a high-level overview of the dual-microcontroller architecture for the Open Reactor Control System. It is intended to give context to developers and AI assistants working on the codebase.

The system is built on a **separation of concerns** principle, dividing tasks between a high-level **System Controller** and a real-time **I/O Controller**.

---

## 1. System Controller (RP2040): The "Brain"

This is the primary MCU for this codebase (`orc-sys-mcu`). It is responsible for all high-level tasks that are not strictly real-time.

*   **Hardware:** Raspberry Pi RP2040
*   **Core Philosophy:** Manages the user experience, network connectivity, and overall system state. It decides *what* should happen and *when*, but delegates the real-time execution to the I/O Controller.

### Primary Responsibilities:
*   **User Interface:** Hosts a web server (using `W5500lwIP` and `WebServer`) to serve the HTML, CSS, and JavaScript front-end.
*   **Network Management:** Handles all network communication, including:
    *   Ethernet connectivity (`lwIP`).
    *   REST APIs for UI interaction (`WebServer`, `ArduinoJson`).
    *   MQTT client for data publishing to the cloud/digital twin (`PubSubClient`).
    *   NTP for time synchronization.
*   **Data Logging & Storage:** Manages the SD card (`SdFat`) for logging system events and sensor data.
*   **High-Level Orchestration:** Runs the main logic for control sequences, recipes, and system modes.
*   **Configuration Management:** Loads and saves system settings (e.g., network, MQTT) from configuration files on the built-in LittleFS.
*   **Inter-Processor Communication (IPC):** Acts as the master in the communication link with the I/O Controller.

### RP2040 Dual-Core Usage:
*   **Core 0:** Dedicated to network-related tasks (`manageNetwork`, `manageMqtt`, WebServer client handling). This prevents network activity from blocking other system functions.
*   **Core 1:** Handles all other management tasks (status LEDs, power monitoring, terminal commands, IPC, SD card).

---

## 2. I/O Controller (SAMD51): The "Hands"

The I/O Controller is a separate MCU (with its own codebase) that handles all direct, real-time interactions with the physical world.

*   **Hardware:** SAM D51 (or similar powerful MCU)
*   **Core Philosophy:** Provides a hardware abstraction layer. It executes low-level commands received from the System Controller and reports back with sensor data and status. It is lean, fast, and deterministic.

### Primary Responsibilities:
*   **Real-Time Control Loops:** Executes time-critical PID loops for temperature, pH, etc.
*   **Direct Sensor Reading:** Interfaces directly with sensors via I2C, SPI, ADC, and one-wire protocols.
*   **Direct Actuator Control:** Manages PWM outputs for heaters, pumps, and valves, and digital outputs for other components.
*   **Hardware Safety Interlocks:** Implements low-level safety checks that must run without interruption (e.g., over-temperature cutoff).
*   **Inter-Processor Communication (IPC):** Acts as the slave, responding to requests from the System Controller.

---

## 3. Inter-Processor Communication (IPC): The "Nervous System"

The two MCUs communicate via a well-defined protocol.

*   **Physical Layer:** A dedicated UART bus (`Serial1` on the RP2040).
*   **Protocol:** A custom, packet-based protocol defined in `lib/IPCprotocol/`.
    *   **`IPCProtocol.h` / `.cpp`:** Implements the message framing, CRC checking, and dispatching.
    *   **`IPCDataStructs.h`:** Defines the exact data structures for all sensor readings and control setpoints. This file is **shared** between both the System Controller and I/O Controller projects to ensure they speak the same language.

**Example Data Flow (Setting Temperature):**
1.  User sets a new temperature in the web UI.
2.  The browser sends a `POST` request to the RP2040's web server.
3.  The RP2040 API handler parses the request.
4.  The RP2040 creates a `TemperatureControl` struct with the new setpoint.
5.  The RP2040 sends this struct to the SAMD51 using the IPC protocol.
6.  The SAMD51 receives the IPC message, validates it, and updates its internal PID loop setpoint.
7.  The SAMD51 periodically reads the temperature sensor and sends a `TemperatureSensor` struct back to the RP2040 via IPC.
8.  The RP2040 receives the sensor data, updates its global state, and makes it available to the UI (via WebSockets/REST) and MQTT.

This architecture ensures that the user interface and network stack remain responsive, even when the I/O Controller is busy with a high-frequency control task.
