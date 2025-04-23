### 1. Alicat MFC (Mass Flow Controllers/Meters)

The addresses below are the **0-based Modbus addresses** as defined in the Alicat documentation, which distinguish between a 1-based "Register Number" and a 0-based "Register Address". The tables in the Alicat manual list the Register Addresses.

| Data Type        | Function                 | Starting Modbus Address (decimal, 0-based) | Number of Registers | Data Format           | Access | Source     |
| :--------------- | :----------------------- | :----------------------------------------- | :------------------ | :-------------------- | :----- | :--------- |
| Float            | Current Pressure Reading | **1353**                                   | 2                   | IEEE 734 Float        | Read   |       |
| Float            | Current Temperature Reading| **1359**                                 | 2                   | IEEE 734 Float        | Read   | [Previous] |
| Float            | Current Mass Flow Reading| **1363**                                   | 2                   | IEEE 734 Float        | Read   | [Previous] |
| Float            | Current Volumetric Flow Reading| **1361**                             | 2                   | IEEE 734 Float        | Read   | [Previous] |
| Float            | Current Setpoint (Controllers)| **1349**                              | 2                   | IEEE 734 Float        | R/W    |       |
| Float            | Current Totalizer 1 Reading| **1365**                                 | 2                   | IEEE 734 Float        | Read   | [Previous] |
| Integer / Float  | Optimized Readings       | **999** (Integer), **1019** (Float)        | 20 (each block)     | Varies (see manual)   | Read   ||
| Integer          | Gas Number               | **1246**                                   | 1                   | Unsigned 16-bit Int   | Read   |       |
| Integer          | Totalizer Overrange Status| **2058**                                | 1                   | 16-bit Int (0=inactive)| Read   | [Previous] |
| Float            | Current Pressure Reading | **2040**                                   | 2                   | IEEE 734 Float        | Read   |       |
| Float            | Current Temperature Reading| **2042**                                 | 2                   | IEEE 734 Float        | Read   |       |
| Float            | Current Volumetric Flow Reading| **2044**                             | 2                   | IEEE 734 Float        | Read   |       |
| Float            | Current Mass Flow Reading| **2046**                                   | 2                   | IEEE 734 Float        | Read   |       |
| Float            | Current Setpoint (Controllers)| **2048**                              | 2                   | IEEE 734 Float        | Read   |       |
| Float            | Current Totalizer 1 Reading| **2050**                                 | 2                   | IEEE 734 Float        | Read   |       |

*   **Note A:** The specific Modbus register addresses for Float format current readings (like 1349, 2040, etc.) are shown in the source excerpts, and these align with the 0-based "Register Address" concept described in the Alicat manual. Addresses 1359, 1361, 1363, 1365 were inferred from the manual's structure in the previous conversation turn, applying the -1 offset to the listed 1-based Register Numbers near the others.

### 2. Dencytee RS485 Sensors

These sensors communicate via Modbus RTU over RS485. The "Start register" values listed in the Dencytee manual are **1-based register numbers**. For compatibility with Modbus master software using **0-based addressing**, these addresses have been **decreased by 1**.

| Data Type        | Function                      | Starting Modbus Address (decimal, 0-based) | Number of Registers | Data Format                     | Access | Source     |
| :--------------- | :---------------------------- | :----------------------------------------- | :------------------ | :------------------------------ | :----- | :--------- |
| Varies           | TCD (Primary Measurement Channel 1) | **2089**?                                  | 10                  | 10-reg block (Float Value, Unit, Status, Limits, etc.) | Read   | [Note B, 44]   |
| Varies           | **Temperature** (PMC6)        | **2409**                                   | 10                  | 10-reg block (Float Value, Unit, Status, Limits, etc.) | Read   |   |
| Varies           | Transmission (SMC13)          | **2855**                                   | 10                  | 10-reg block (Float Value, Unit, Status, Limits, etc.) | Read   | [Previous] |
| Varies           | Reflection (SMC10)            | **2759**                                   | 10                  | 10-reg block (Float Value, Unit, Status, Limits, etc.) | Read   |       |
| Varies           | Reflection (SMC14)            | **2887**                                   | 10                  | 10-reg block (Float Value, Unit, Status, Limits, etc.) | Read   |       |
| Float            | Operating Temperature Range   | **4607**                                   | 4                   | Float (Min/Max in °C)           | Read   |       |
| Float            | Measurement Temperature Range | **4611**                                   | 4                   | Float (Min/Max in °C)           | Read   |       |
| Float            | Calibration Temperature Range | **4615**                                   | 4                   | Float (Min/Max in °C)           | Read   |       |
| Float            | User Defined Measurement Temp | **4623**                                   | 4                   | Float (Min/Max in °C)           | R/W    |       |
| Integer          | Device Address                | **4095**                                   | 2                   | Unsigned 16-bit Int   | R/W    |       |
| Integer          | Baud Rate Code                | **4104**                                   | 2                   | Coded Value (see manual) | R/W    | [Previous] |
| Long Int         | Interface Parameter           | **4107**                                   | 4                   | Coded Value (see manual) | R/W    |       |
| Unsigned Int     | Available PMCs/SMCs           | **2047**                                   | 2                   | Bitwise Coded Value   | Read   |       |
| Integer          | System Time Counter           | **8231**                                   | 2                   | Unsigned 32-bit (approx)| R/W    |   |
| Long Int         | TCD Offset Value              | **3145**                                   | 4                   | Float                 | R/W    |   |
| Bitwise Int      | Mark/Clear Zero TCD Cmd       | **41245**                                  | 4                   | Coded Value (0/1)     | W/R    |       |
| ASCII            | Inoculation Label             | **41289**                                  | 8                   | 16 ASCII Chars        | Read   |       |
| Bitwise Int      | Inoculate Command             | **41299**                                  | 4                   | Coded Value (0/1)     | W/R    |       |
| Bitwise Int      | Available CoP for PMC1        | **10259**                                  | 2                   | Bitwise Coded Value   | R/W    |       |
| Integer          | Selected CoP Unit             | **10273**                                  | 2                   | Coded Value (Unit)    | R/W    |       |
| Bitwise Int      | Correlation Preview Init      | **10263**                                  | 2                   | Coded Value           | W/R    |   |
| Bitwise Int      | Correlation Preview Status    | **10265**                                  | 2                   | Bitwise Coded Status  | Read   |   |
| Bitwise Int      | Save Correlation Preview Cmd  | **10265**                                  | 2                   | Coded Value (User Space) | W      |   |
| Bitwise Int      | Delete Correlation Cmd        | **10263**                                  | 2                   | Coded Value (User Space) | W      |   |
| Varies           | Correlation Point Parameters (e.g., Assigned Value, Temp, Trans, Reflect) | **10359**, **10391**, **10647**, **10679** + (CoPx-1)*720 | 8 (each)            | Varies (Unit, Value, Min/Max, ASCII) | Read   |       |
| Varies           | Correlation Point Values (e.g., Value 2, Value 3, Value 11, Value 12) | **10373**, **10411**, **10661**, **10693** + (CoPx-1)*720 | 6 (each)            | Varies (Unit, Value 2) | Read   |       |
| Varies           | Correlation Point Raw Data (Transmission, Reflection, Input Value) | **44485** + (n-1)*54 (n=1-10) | 6 (each) | Varies (Value) | Read   |       |
| Long Int         | Operating Hours               | **4675**                                   | 6                   | Float (h)             | Read   |       |
| Long Int         | Power Cycles etc.             | **4681**                                   | 6                   | Count                 | Read   |       |
| Long Int         | SIP/CIP Cycles                | **4687**                                   | 4                   | Count                 | Read   |       |
| Integer          | Autoclavings Count            | **4691**                                   | 2                   | Count                 | R/W    |   |
| ASCII            | User Space 1 (Measuring Point Description) | **1599**                  | 8                   | 16 ASCII Chars        | Read   |  |
| ASCII            | Free User Space (various)     | **1687** to **1751** by 8 | 8 (each block)      | 16 ASCII Chars        | Read   |  |
| Integer          | Recall Command                | **8191**                                   | 2                   | Coded Value (911)     | Write  |  |

*   **Note B:** The provided Dencytee excerpts do not explicitly show the starting Modbus address for reading the PMC1 (TCD) measurement value block directly. Based on the structure of Hamilton Arc sensor manuals (e.g., VisiFerm DO [Previous]) which place PMC1 at register 2090 (1-based), this address is inferred as likely, leading to **2089** (0-based), but needs verification from the full Dencytee manual.
*   **Previous** source refers to addresses extracted in the previous conversation turn that are also present in the current sources but not explicitly cited in the new excerpts for that specific line item (e.g., Baud Rate, Transmission, Reflection, etc.).

### 3. VisiFerm DO / VisiFerm DO Arc Sensors and pH Arc Sensors

These are Hamilton Arc sensors. The "Start register" values listed in their manuals are **1-based register numbers**. For compatibility with Modbus master software using **0-based addressing**, these addresses have been **decreased by 1**.

**VisiFerm DO / VisiFerm DO Arc Sensors:**

| Data Type        | Function                      | Starting Modbus Address (decimal, 0-based) | Number of Registers | Data Format                     | Access | Source     |
| :--------------- | :---------------------------- | :----------------------------------------- | :------------------ | :------------------------------ | :----- | :--------- |
| Varies           | Oxygen (PMC1)                 | **2089**                                   | 10                  | 10-reg block (Float Value, Unit, Status, Limits, etc.) | Read   |       |
| Varies           | **Temperature** (PMC6)        | **2409**                                   | 10                  | 10-reg block (Float Value, Unit, Status, Limits, etc.) | Read   |       |
| Float            | Operating Temperature Range   | **4607**                                   | 4                   | Float (Min/Max in °C)           | Read   | [71, Previous] |
| Float            | Measurement Temperature Range | **4611**                                   | 4                   | Float (Min/Max in °C)           | Read   | [71, Previous] |
| Float            | Calibration Temperature Range | **4615**                                   | 4                   | Float (Min/Max in °C)           | Read   | [71, Previous] |
| Float            | User Defined Measurement Temp | **4623**                                   | 4                   | Float (Min/Max in °C)           | R/W    | [71, Previous] |
| Integer          | Device Address                | **4095**                                   | 2                   | Unsigned 16-bit Int   | R/W    |       |
| Unsigned Int     | Available Analog Outputs      | **4319**                                   | 2                   | Bitwise Coded Value   | Read   |       |
| Unsigned Int     | Analog Interface Mode AO1     | **4359**                                   | 2                   | Coded Value (0x01, 0x02, etc.) | R/W    | [80, Previous] |
| Unsigned Int     | Available PMCs for AO1        | **4361**                                   | 2                   | Bitwise Coded Value   | Read   |       |
| Unsigned Int     | Selected PMC for AO1          | **4363**                                   | 2                   | Coded Value (0x01 for PMC1, 0x20 for PMC6) | R/W    |       |
| Unsigned Int     | Available Units PA9 (Moving Avg) | **3367**                                | 2                   | Bitwise Coded Value   | Read   |       |
| Varies           | Set PA9 (Moving Avg) Value/Unit | **3369**                                  | 4                   | Bitwise Unit / Integer Value | W/R    |       |
| Unsigned Int     | Available Units PA10 (Sub-Measurements) | **3400**                           | 2                   | Bitwise Coded Value   | Read   |       |
| Varies           | Set PA10 (Sub-Measurements) Value/Unit | **3401**                          | 4                   | Bitwise Unit / Integer Value | W/R    |   |
| Varies           | Read PA10 Values              | **3401**                                   | 8                   | Unit, Value, Min/Max (Float) | Read   |       |
| Unsigned Int     | Available Units PA11 (Min Resolution) | **3431**                            | 2                   | Bitwise Coded Value   | Read   |       |
| Varies           | Set PA11 (Min Resolution) Value/Unit | **3433**                           | 4                   | Bitwise Unit / Integer Value | W/R    |       |
| Varies           | Read PA11 Values              | **3433**                                   | 8                   | Unit, Value, Min/Max (Float) | Read   |       |
| Unsigned Int     | Available Units PA13 (Meas Interval) | **3495**                            | 2                   | Bitwise Coded Value   | Read   |       |
| Varies           | Set PA13 (Meas Interval) Value/Unit | **3497**                          | 4                   | Bitwise Unit / Integer Value | W/R    |       |
| Unsigned Int     | Available Units PA14 (Sensor Cap Part Nr) | **3527**                        | 2                   | Bitwise Coded Value   | Read   |       |
| Varies           | Set PA14 (Sensor Cap Part Nr) Value/Unit | **3529**                       | 4                   | Bitwise Unit / Integer Value | W/R    |       |
| Varies           | Read PA14 Values              | **3529**                                   | 8                   | Unit, Value, Min/Max (Float) | Read   |       |
| Bitwise Int      | Available Calibration Points PMC1 | **5119**                             | 2                   | Bitwise Coded Value   | Read   |       |
| Varies           | Make Calibration CP1 Cmd      | **5161**                                   | 2                   | Float Value           | Write  |       |
| Varies           | Make Calibration CP2 Cmd      | **5193**                                   | 2                   | Float Value           | Write  |       |
| Unsigned Int     | Product Calibration Commands  | **5339**                                   | 2                   | Coded Value (0x01 etc.) | A/S W/R |       |
| Bitwise Int      | Calibration Status CP1        | **5159**                                   | 2                   | Bitwise Coded Status  | Read   | [96, Previous] |
| Long Int         | Operating Hours               | **4675**                                   | 6                   | Float (h)             | Read   |   |
| Long Int         | Power Cycles etc.             | **4681**                                   | 6                   | Count                 | Read   |   |
| Long Int         | SIP/CIP Cycles                | **4687**                                   | 4                   | Count                 | Read   |   |
| Integer          | Autoclavings Count            | **4691**                                   | 2                   | Count                 | R/W    |   |
| Integer          | System Time Counter           | **8231**                                   | 2                   | Unsigned 32-bit (approx)| R/W    |   |
| Integer          | CIP Correction Mode           | **5595**                                   | 2                   | Coded Value (0x00, 0x01, 0x02) | S R/W  |       |
| ASCII            | User Space 1 (Measuring Point Description) | **1599**                  | 8                   | 16 ASCII Chars        | Read   |  |
| Integer          | Recall Command                | **8191**                                   | 2                   | Coded Value (911)     | Write  |  |

**pH Arc Sensors:**

| Data Type        | Function                      | Starting Modbus Address (decimal, 0-based) | Number of Registers | Data Format                     | Access | Source     |
| :--------------- | :---------------------------- | :----------------------------------------- | :------------------ | :------------------------------ | :----- | :--------- |
| Varies           | pH (PMC1)                     | **2089**?                                  | 10                  | 10-reg block (Float Value, Unit, Status, Limits, etc.) | Read   | [Note D, 106, 108, 109] |
| Varies           | **Temperature** (PMC6)        | **2409**                                   | 10                  | 10-reg block (Float Value, Unit, Status, Limits, etc.) | Read   |      |
| Varies           | R glass (SMC1)                | **2471**                                   | 6                   | 6-reg block (Unit, Value, Std Dev) | Read   | [107, Previous] |
| Varies           | R reference (SMC2)            | **2503**                                   | 6                   | 6-reg block (Unit, Value, Std Dev) | Read   | [107, Previous] |
| Varies           | E pH vs. ref (SMC4)           | **2559**                                   | 6                   | 6-reg block (Unit, Value, Std Dev) | Read   | [107, Previous] |
| Varies           | T act (SMC9 - Current Temp)   | **2727**                                   | 6                   | 6-reg block (Unit, Value, Std Dev) | Read   | [107, Previous] |
| Float            | Operating Temperature Range   | **4607**                                   | 4                   | Float (Min/Max in °C)           | Read   | [71, Previous] |
| Float            | Measurement Temperature Range | **4611**                                   | 4                   | Float (Min/Max in °C)           | Read   | [71, Previous] |
| Float            | Calibration Temperature Range | **4615**                                   | 4                   | Float (Min/Max in °C)           | Read   | [71, Previous] |
| Integer          | Device Address                | **4095**                                   | 2                   | Unsigned 16-bit Int   | R/W    |      |
| Unsigned Int     | Analog Interface Mode AO1     | **4359**                                   | 2                   | Coded Value (0x01, 0x02, etc.) | R/W    |      |
| Unsigned Int     | Selected PMC for AO1          | **4363**                                   | 2                   | Coded Value (0x01 for PMC1, 0x20 for PMC6) | R/W    |      |
| Unsigned Int     | Available Units PA9 (Moving Avg) | **3367**                                | 2                   | Bitwise Coded Value   | Read   |      |
| Varies           | Set PA9 (Moving Avg) Value/Unit | **3369**                                  | 4                   | Bitwise Unit / Integer Value | W/R    |      |
| Unsigned Int     | Available Units PA12 (Filter Time) | **3463**                                | 2                   | Bitwise Coded Value   | Read   |      |
| Varies           | Set PA12 (Filter Time) Value/Unit | **3465**                               | 4                   | Bitwise Unit / Integer Value | W/R    |      |
| Bitwise Int      | Available Calibration Points PMC1 | **5119**                             | 2                   | Bitwise Coded Value   | Read   |      |
| Varies           | Limits for CP1 (Min/Max Value) | **5151**                                  | 6                   | Unit / Float Min/Max  | Read   |      |
| Varies           | Limits for CP2 (Min/Max Value) | **5183**                                  | 6                   | Unit / Float Min/Max  | Read   |      |
| Bitwise Int      | Calibration Status CP6        | **5317**                                   | 2                   | Bitwise Coded Status  | Read   |      |
| Varies           | Calibrated Values CP1 (pH, mV, K) | **5519**                                | 8                   | Float                 | Read   |      |
| Varies           | Calibrated Values CP2 (pH, mV, K) | **5527**                                | 8                   | Float                 | Read   |      |
| Varies           | Calibrated Values CP6 (pH, mV, K) | **5559**                                | 8                   | Float                 | Read   |      |
| Varies           | Limits of Offset/Slope        | **5479**                                   | 8                   | Float                 | U/A/S R/W | |
| Integer          | System Time of Calibration CP1| **5181**                                  | 2                   | Unsigned 32-bit (approx)| Read   | |
| Integer          | System Time of Calibration CP2| **5213**                                  | 2                   | Unsigned 32-bit (approx)| Read   | |
| Integer          | System Time of Calibration CP6| **5341**                                  | 2                   | Unsigned 32-bit (approx)| Read   | |
| Bitwise Int      | Available Calibration Standard Sets | **9471**                             | 2                   | Bitwise Coded Value   | Read   |      |
| Varies           | Calibration Standard Values (Nominal/Tolerance) | **9535**, **9551**,... | 8                   | Float                 | Read   |      |
| Bitwise Int      | Availability/Selection of Calibration Standards (Manual) | **9527**         | 2                   | Bitwise Coded Value   | Read   |      |
| Bitwise Int      | Availability/Selection of Calibration Standards (Automatic) | **9529**     | 2                   | Bitwise Coded Value   | Read   |      |
| Long Int         | Operating Hours               | **4675**                                   | 6                   | Float (h)             | Read   | |
| Long Int         | Power Cycles etc.             | **4681**                                   | 6                   | Count                 | Read   | |
| Long Int         | SIP/CIP Cycles                | **4687**                                   | 4                   | Count                 | Read   | |
| Integer          | Autoclavings Count            | **4691**                                   | 2                   | Count                 | R/W    | |
| Integer          | System Time Counter           | **8231**                                   | 2                   | Unsigned 32-bit (approx)| R/W    | |
| ASCII            | User Space 1 (Measuring Point Description) | **1599**                  | 8                   | 16 ASCII Chars        | Read   |  |
| ASCII            | Free User Space (various)     | **1719** to **1751** by 8 | 8 (each block)      | 16 ASCII Chars        | Read   |      |
| Integer          | Recall Command                | **8191**                                   | 2                   | Coded Value (911)     | Write  |  |

*   **Note C (Dencytee & VisiFerm DO):** The provided Dencytee and VisiFerm DO excerpts do not explicitly show the starting Modbus address for reading the PMC1 (TCD or Oxygen) measurement value block directly, but reference it in relation to other parameters like analog output mapping. Based on the structure of Hamilton Arc sensor manuals, this block starts at register 2090 (1-based), leading to **2089** (0-based) for the main measurement block (TCD for Dencytee, Oxygen for VisiFerm DO). This needs verification from the full Dencytee manual for TCD PMC1.
*   **Note D (pH Arc):** The provided pH Arc excerpts do not explicitly show the starting Modbus address for reading the PMC1 (pH) measurement value block directly as a reading example, but reference it in relation to other parameters like analog output mapping. Based on the structure of Hamilton Arc sensor manuals (e.g., VisiFerm DO [Previous]) which place PMC1 at register 2090 (1-based), this address is inferred as likely, leading to **2089** (0-based), but needs verification from the full pH Arc manual.
*   **Previous** source refers to addresses extracted in the previous conversation turn that are also present in the current sources but not explicitly cited in the new excerpts for that specific line item.

Always refer to the specific device manual and your Modbus master software documentation to confirm the correct addressing convention (0-based vs. 1-based) and register definitions for your application.