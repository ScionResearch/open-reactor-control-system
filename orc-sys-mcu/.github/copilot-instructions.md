### 📝 Project Context

- **Read `PLANNING.md`** for the dual-MCU architecture and the role of this RP2040 controller.
- **Check `TASK.md`** before starting any task to understand the immediate goal.

### 🛠️ PlatformIO Workflow

- **Use VS Code's built-in PlatformIO extension for all builds and uploads.**
- **Build project:** Use VS Code Task "PlatformIO: Build" or the PlatformIO toolbar
- **Upload firmware:** Use VS Code Task "PlatformIO: Upload" or the PlatformIO toolbar
- **View serial logs:** Use VS Code Task "PlatformIO: Monitor" or the PlatformIO toolbar
- **Clean build files:** Use VS Code Task "PlatformIO: Clean" or the PlatformIO toolbar
- **Alternative CLI commands (if needed):**
  - Build: `C:\Users\vanderwt\.platformio\penv\Scripts\platformio.exe run`
  - Upload: `C:\Users\vanderwt\.platformio\penv\Scripts\platformio.exe run -t upload`
  - Monitor: `C:\Users\vanderwt\.platformio\penv\Scripts\platformio.exe device monitor`
  - Clean: `C:\Users\vanderwt\.platformio\penv\Scripts\platformio.exe run -t clean`
- **Manage libraries via `platformio.ini` only.** Do not install them manually.

### 🧪 Testing Strategy

- **Write Unit Tests** for pure logic (e.g., data manipulation, calculations) in the `/test` directory.
- **Perform End-to-End (E2E) Hardware Testing for all features.** This involves:
    1. Uploading the firmware to the RP2040.
    2. Using the **Serial Monitor** to observe logs and send commands.
    3. Interacting with the **Web UI** to trigger actions and view status.
    4. Using external tools like **MQTT Explorer** or Python scripts to verify network functionality.
- **Test each function/class/endpoint for:**
    - Happy path (expected behavior)
    - Edge cases (e.g., zero, max values, empty strings)
    - Failure cases (e.g., invalid input, disconnections)
- **Update tests after refactoring.**

### ✅ Workflow

- **Mark tasks as done in `TASK.md`** with `[x]` after successful E2E testing.
- **Commit after each logical feature or fix** with clear messages following conventional commit standards (e.g., `feat:`, `fix:`, `docs:`).

### 📎 Style & Conventions

- **Language: Modern C++** as supported by the RP2040 toolchain.
- **Formatting: Use `clang-format`.** A `.clang-format` file should define the project style.
- **Data Models:**
    - Use `structs` in `IPCDataStructs.h` for inter-processor data.
    - Use `ArduinoJson` for serialization/deserialization of API and MQTT payloads.
- **Core Libraries:**
    - `WebServer` for REST APIs.
    - `PubSubClient` for MQTT.
    - `SdFat` for SD card access.
    - `W5500lwIP` for Ethernet.

### 📚 Documentation

- **Update `README.md` or `SYSTEM_OVERVIEW.md`** for any architectural, setup, or feature changes.
- **Use Doxygen-style docstrings (`/** ... */`)** for all functions, classes, and headers.
- **Comment complex logic with `// Reason:` explanations** to clarify *why* the code is written a certain way.

### 🔄 Sync & Checklist

- **Update all relevant parts of the system when making a change.**
- **Checklist for feature changes:**
    - [ ] C++ code updated (`.h` and `.cpp`).
    - [ ] `IPCDataStructs.h` updated if communication changes.
    - [ ] API handler in `network.cpp` updated.
    - [ ] MQTT topics/payloads in `mqttManager.cpp` updated.
    - [ ] Web UI (`.js` file) updated if needed.
    - [ ] Documentation (docstrings, READMEs) updated.
    - [ ] E2E test plan passed.

### 🧠 AI Rules

- **No hallucinations:** Confirm files/modules exist before referencing them. The full codebase is in your context.
- **Ask if the task is unclear.**
- **Do not overwrite existing logic unless `TASK.md` explicitly says so.** Refactor or add new functions instead.
- **Plan before coding; reflect on the plan after.**
- **Maintain architectural purity:** Keep network logic on Core 0 and delegate real-time I/O tasks conceptually to the (separate) I/O controller via IPC.