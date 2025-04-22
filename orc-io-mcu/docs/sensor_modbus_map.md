# Sensor Modbus Register Map

This document outlines the Modbus communication details for various sensors used in the Open Reactor Control System.

**Note:** All information must be verified against the official manufacturer documentation for each specific sensor model.

---

## 1. Dissolved Oxygen (DO) Sensor

*   **Manufacturer/Model:** Hamilton VisiFerm DO Arc [Verify Specific Model]
*   **Protocol:** Modbus RTU
*   **Default Slave Address:** 1 (Verify in sensor settings/manual)
*   **Baud Rate:** [Specify, e.g., 19200] (Verify)
*   **Data Bits:** 8 (Common default, verify)
*   **Parity:** [Specify, e.g., None, Even] (Verify)
*   **Stop Bits:** [Specify, e.g., 1, 2] (Verify)

### Registers (Function Code 0x03 - Read Holding Registers)

*   **Primary Measurement Channel 1 (Oxygen):**
    *   Read **10 registers** starting at address **2089 (0x0829)**.
    *   This block contains: Physical Unit, Measurement Value, Status, Min Limit, Max Limit. Data types (Float, Hex, etc.) and order need verification from the manual.
*   **Primary Measurement Channel 6 (Temperature):**
    *   Read **10 registers** starting at address **2409 (0x0969)**.
    *   This block contains: Physical Unit, Measurement Value, Status, Min Limit, Max Limit. Data types and order need verification.
*   **Temperature Ranges:**
    *   Operating Range (Min/Max): Registers starting at **4607 (0x11FF)**
    *   Measurement Range (Min/Max): Registers starting at **4611 (0x1203)**
    *   Calibration Range (Min/Max): Registers starting at **4615 (0x1207)**
    *   User Defined Measurement Range (Min/Max): Registers starting at **4623 (0x120F)**

**Notes:**
*   The addresses `0x0829` and `0x0969` previously found in `drv_do_sensor.cpp` now match the adjusted VisiFerm documentation examples (assuming 0-based addressing). **Verification against the specific manual is still crucial.**
*   Reading requires processing a block of 10 registers for each primary measurement.

---

## 2. pH Sensor

*   **Manufacturer/Model:** Hamilton pH Arc [Verify Specific Model]
*   **Protocol:** Modbus RTU
*   **Default Slave Address:** [If Modbus - Verify]
*   **Baud Rate:** [If Modbus - Verify]
*   **Data Bits:** [If Modbus - Verify]
*   **Parity:** [If Modbus - Verify]
*   **Stop Bits:** [If Modbus - Verify]

### Registers (Function Code 0x03 - Read Holding Registers)

*   **Primary Measurement Channel 1 (pH):**
    *   Likely read **10 registers** starting at address **2089 (0x0829)** (similar structure to VisiFerm DO).
    *   This block likely contains: Physical Unit, Measurement Value, Status, Min Limit, Max Limit. Verify details from the manual.
*   **Primary Measurement Channel 6 (Temperature):**
    *   Read **10 registers** starting at address **2409 (0x0969)**.
    *   This block contains: Physical Unit, Measurement Value, Status, Min Limit, Max Limit. Verify details.
*   **Secondary Measurement Channels (SMC):**
    *   Measurement values (Unit, Value, Std Dev) start at:
        *   SMC1 (R glass): **2471 (0x09A7)**
        *   SMC2 (R reference): **2503 (0x09C7)**
        *   ... (up to SMC9) ...
        *   SMC9 (T act - Current Temperature): **2727 (0x0AA7)**
*   **Temperature Ranges:**
    *   Operating Range (Min/Max): Registers starting at **4607 (0x11FF)**
    *   Measurement Range (Min/Max): Registers starting at **4611 (0x1203)**
    *   Calibration Range (Min/Max): Registers starting at **4615 (0x1207)**

**Notes:**
*   Reading primary channels requires processing a block of 10 registers.
*   Secondary channels provide additional diagnostic values.

---

## 3. Optical Density (OD) Sensor

*   **Manufacturer/Model:** Hamilton Dencytee [Verify Specific Model]
*   **Protocol:** Modbus RTU
*   **Default Slave Address:** [If Modbus - Verify]
*   **Baud Rate:** [If Modbus - Verify]
*   **Data Bits:** [If Modbus - Verify]
*   **Parity:** [If Modbus - Verify]
*   **Stop Bits:** [If Modbus - Verify]

### Registers (Function Code 0x03 - Read Holding Registers)

*   **Primary Measurement Channel 1 (TCD - Total Cell Density):**
    *   Measurement value is within a **10-register block**. The exact starting address needs to be confirmed from the Dencytee manual. (Similar structure to VisiFerm's **2089 (0x0829)** block is possible but not confirmed).
*   **Primary Measurement Channel 6 (Temperature):**
    *   Measurement value is within a **10-register block**. The exact starting address needs to be confirmed. (VisiFerm uses **2409 (0x0969)**, which might be similar).
*   **Secondary Measurement Channels (Transmission, Reflection, etc.):**
    *   Specific register addresses need to be confirmed from the manual.
*   **Temperature Ranges:**
    *   Operating Range (Min/Max): Registers starting at **4607 (0x11FF)**
    *   Measurement Range (Min/Max): Registers starting at **4611 (0x1203)**
    *   Calibration Range (Min/Max): Registers starting at **4615 (0x1207)**
    *   User Defined Measurement Range (Min/Max): Registers starting at **4623 (0x120F)**

**Notes:**
*   Reading primary channels requires processing a block of 10 registers. Exact starting addresses and block contents must be verified.

---

## 4. Flow Sensor / Mass Flow Controller (MFC)

*   **Manufacturer/Model:** Alicat MFC [Verify Specific Model]
*   **Protocol:** Modbus RTU
*   **Default Slave Address:** [Verify, often configurable]
*   **Baud Rate:** [Verify]
*   **Data Bits:** [Verify]
*   **Parity:** [Verify]
*   **Stop Bits:** [Verify]

### Registers (Function Code 0x03 - Read Holding Registers, unless noted)

*   **Optimized Reading Registers (Recommended for efficiency):**
    *   Integer Format Block: **1000-1019** (Contains multiple scaled values)
    *   Float Format Block: **1020-1039** (Contains multiple engineering unit values)
    *   *Refer to Alicat manual for the exact layout within these blocks.*

*   **Individual Parameter Registers:**
    *   **Pressure:**
        *   `1304-1305`: Scaled Integer (4 bytes)
        *   `1354-1355`: Engineering Units (Float32)
        *   `2041-2042`: Engineering Units (Float32)
    *   **Temperature:**
        *   `1310-1311`: Scaled Integer (4 bytes)
        *   `1360-1361`: Engineering Units (Float32)
        *   `2043-2044`: Engineering Units (Float32)
    *   **Volumetric Flow:**
        *   `1312-1313`: Scaled Integer (4 bytes)
        *   `1362-1363`: Engineering Units (Float32)
        *   `2045-2046`: Engineering Units (Float32)
    *   **Mass Flow:**
        *   `1314-1315`: Scaled Integer (4 bytes)
        *   `1364-1365`: Engineering Units (Float32)
        *   `2047-2048`: Engineering Units (Float32)
    *   **Setpoint (Read/Write - Use Function Code 0x06/0x10):**
        *   `1300-1301`: Scaled Integer (4 bytes)
        *   `1350-1351`: Engineering Units (Float32)
        *   `2049-2050`: Engineering Units (Float32)
    *   **Totalizer 1:**
        *   `1366-1367`: Engineering Units (Float32)
        *   `2051-2052`: Engineering Units (Float32)

*   **Temperature Information/Configuration:**
    *   Temperature Info Block Start: **1700 (0x06A4)** (Provides type, source, min/max, units, etc.)
    *   Standard Flow Ref Temp (Integer): **11 (0x000B)** (Register 53 in some contexts?)
    *   Normal Flow Ref Temp (Integer): **12 (0x000C)** (Register 54 in some contexts?)
    *   Standard Flow Ref Temp (Float): **54 (0x0036)**
    *   Normal Flow Ref Temp (Float): **55 (0x0037)**

**Notes:**
*   Alicat provides multiple registers for the same data in different formats (scaled integer vs. float). Using the float format (engineering units) is often easier.
*   Using the optimized reading registers (1000-1039) is generally more efficient than reading individual parameters.
*   Writing setpoints requires Modbus function code 0x06 (Write Single Register - for integers if applicable) or 0x10 (Write Multiple Registers - for floats).

---

## 5. Pressure Sensor

*   **Manufacturer/Model:** [Specify if applicable - Could be Alicat MFC, or separate sensor]
*   **Protocol:** [Specify, e.g., Analog, Modbus RTU]
*   ... (Add details as for DO/pH if Modbus) ...

### Registers (If Modbus)

| Parameter          | Address (Hex) | Address (Dec) | Data Type | Registers | Scaling / Notes |
| :----------------- | :------------ | :------------ | :-------- | :-------- | :-------------- |
| Pressure Value     | `0x????`      | ????          | [...]     | [...]     | [See Alicat section if applicable] |
| *Other Parameters* | `0x????`      | ????          | [...]     | [...]     | [...]           |

---

**General Note on Hamilton Sensors (Dencytee, VisiFerm, pH Arc):**
To get a complete set of measurement data for a primary channel (e.g., DO, pH, Temp, TCD), you often need to read a block of 10 registers starting at the channel's base address (e.g., **2089** for PMC1, **2409** for PMC6, assuming 0-based addressing) and then interpret the different parts within that block according to their data types (Hex, Float) and meanings as defined in the specific sensor manual.
