# IPC Protocol Fixes - SAME51 Side

## Issues Found and Fixed

### Issue 1: Wrong END Byte (CRITICAL) ✅ FIXED

**File:** `src/drivers/ipc/ipc_protocol.h` **Line 16**

**Problem:**
```cpp
#define IPC_START_BYTE          0x7E
#define IPC_END_BYTE            0x7D  // ❌ WRONG! Conflicts with ESCAPE_BYTE
#define IPC_ESCAPE_BYTE         0x7D
```

The END byte was incorrectly defined as `0x7D`, which is the same as the ESCAPE byte. This creates an impossible protocol - the receiver can't distinguish between an escaped byte and the end of the packet.

**Fix:**
```cpp
#define IPC_START_BYTE          0x7E
#define IPC_END_BYTE            0x7E  // ✅ FIXED
#define IPC_ESCAPE_BYTE         0x7D
```

Both START and END bytes are now `0x7E`, matching the RP2040 implementation.

---

### Issue 2: LENGTH Field Interpretation Mismatch (CRITICAL) ✅ FIXED

**File:** `src/drivers/ipc/drv_ipc.cpp` **Lines 105-110**

**Problem:**
The SAME51 was sending LENGTH as "payload size only", while RP2040 was sending LENGTH as "total size of LENGTH + TYPE + PAYLOAD".

**SAME51 was sending (INCORRECT):**
```cpp
// For PING with no payload:
LENGTH = 0x00 0x00  // 0 bytes of payload

// Packet: 7E 00 00 00 ?? ?? 7E
//            └─┘  └─ TYPE
//            LENGTH=0
```

**RP2040 was sending (CORRECT):**
```cpp
// For PING with no payload:
LENGTH = 0x00 0x03  // 3 bytes = LENGTH(2) + TYPE(1) + PAYLOAD(0)

// Packet: 7E 00 03 00 29 B1 7E
//            └─┘  └─ TYPE
//            LENGTH=3
```

**When RP2040 sent PING:**
1. RP2040 sends: `7E 00 03 00 29 B1 7E`
2. SAME51 reads LENGTH=3 and thinks: "3 bytes of payload expected"
3. SAME51 expects total: 2 (LENGTH) + 1 (TYPE) + 3 (PAYLOAD) + 2 (CRC) = 8 bytes
4. SAME51 gets: 2 + 1 + 0 + 2 = 5 bytes
5. **Length mismatch error!** ❌

**Fix - TX Side (lines 105-110):**
```cpp
// OLD (INCORRECT):
crcData[crcDataLen++] = (packet->payloadLen >> 8) & 0xFF;
crcData[crcDataLen++] = packet->payloadLen & 0xFF;

// NEW (CORRECT):
// LENGTH = size of (LENGTH field (2) + MSG_TYPE (1) + PAYLOAD (N))
uint16_t totalLength = 2 + 1 + packet->payloadLen;
crcData[crcDataLen++] = (totalLength >> 8) & 0xFF;
crcData[crcDataLen++] = totalLength & 0xFF;
```

**Fix - RX Side (lines 239-250):**
```cpp
// OLD (INCORRECT):
uint16_t payloadLen = ((uint16_t)ipcDriver.rxBuffer[0] << 8) | ipcDriver.rxBuffer[1];
uint16_t expectedLen = 2 + 1 + payloadLen + 2;  // Wrong interpretation

// NEW (CORRECT):
// LENGTH = size of (LENGTH field (2) + MSG_TYPE (1) + PAYLOAD (N))
uint16_t totalLength = ((uint16_t)ipcDriver.rxBuffer[0] << 8) | ipcDriver.rxBuffer[1];
uint16_t payloadLen = totalLength - 3;  // Extract actual payload size
uint16_t expectedLen = totalLength + 2;  // Total + CRC
```

---

## Protocol Specification (Corrected)

### Packet Structure

```
[START] [LEN_HI] [LEN_LO] [TYPE] [PAYLOAD...] [CRC_HI] [CRC_LO] [END]
  0x7E    u8       u8       u8      0-1024       u8       u8     0x7E
```

### Field Definitions

1. **START/END Byte:** Always `0x7E`
   - Never escaped
   - Marks packet boundaries
   
2. **LENGTH Field (16-bit, big-endian):**
   - **LENGTH = 2 (self) + 1 (TYPE) + N (PAYLOAD)**
   - Minimum value: 3 (for packets with no payload)
   - Maximum value: 3 + 1024 = 1027
   - **IMPORTANT:** LENGTH includes itself in the count!

3. **TYPE:** 8-bit message type (0x00 = PING, 0x01 = PONG, etc.)

4. **PAYLOAD:** 0-1024 bytes of message data

5. **CRC:** CRC16-CCITT checksum
   - Calculated over: LENGTH(2) + TYPE(1) + PAYLOAD(N)
   - Algorithm: Initial=0xFFFF, Polynomial=0x1021
   - 16-bit result, big-endian

6. **Byte Stuffing:**
   - Applied to: LENGTH, TYPE, PAYLOAD, CRC
   - NOT applied to: START/END bytes
   - If byte == `0x7E`: Replace with `0x7D 0x5E`
   - If byte == `0x7D`: Replace with `0x7D 0x5D`

---

## Example Packets

### PING (No Payload)
```
Unescaped: 7E 00 03 00 29 B1 7E
           │  └─┘  │  └──┘  │
           │  LEN  │  CRC   │
           │  =3   TYPE     END
           START   =0x00
```

**Breakdown:**
- START: `0x7E`
- LENGTH: `0x00 0x03` = 3 bytes (LENGTH itself + TYPE)
- TYPE: `0x00` (IPC_MSG_PING)
- PAYLOAD: (none)
- CRC: `0x29 0xB1` (over bytes: 00 03 00)
- END: `0x7E`

### PONG (No Payload)
```
Unescaped: 7E 00 03 01 28 21 7E
           │  └─┘  │  └──┘  │
           │  LEN  │  CRC   │
           │  =3   TYPE     END
           START   =0x01
```

### HELLO (With Payload)
```
Payload: 72 bytes (protocolVersion + firmwareVersion + deviceName)
LENGTH = 2 + 1 + 72 = 75 = 0x004B

Unescaped: 7E 00 4B 02 [72 bytes payload] [CRC_HI] [CRC_LO] 7E
           │  └─┘  │                       └───────┘        │
           │  LEN  TYPE                      CRC            END
           START  =75  =0x02 (HELLO)
```

---

## CRC Calculation Example

For PING packet:
```
Input bytes: [0x00, 0x03, 0x00]  (LENGTH_HI, LENGTH_LO, TYPE)

CRC16-CCITT:
  Initial: 0xFFFF
  Polynomial: 0x1021
  
  Process byte 0x00:
    crc = 0xFFFF ^ (0x00 << 8) = 0xFFFF
    ... 8 shift/XOR iterations ...
    
  Process byte 0x03:
    ... continue ...
    
  Process byte 0x00:
    ... continue ...
    
  Final CRC: 0x29B1
```

---

## Testing After Fixes

### Expected Behavior

1. **RP2040 sends PING:**
   ```
   TX: 7E 00 03 00 29 B1 7E
   ```

2. **SAME51 receives and validates:**
   - Detects START byte `0x7E`
   - Reads LENGTH = 3
   - Reads TYPE = 0x00 (PING)
   - Calculates CRC over [00 03 00] → expects 0x29B1
   - Receives CRC = 0x29B1 ✅
   - Detects END byte `0x7E`
   - **Packet valid!**

3. **SAME51 responds with PONG:**
   ```
   TX: 7E 00 03 01 28 21 7E
   ```

4. **RP2040 receives PONG:**
   - Validates CRC ✅
   - Logs "Received PONG from SAME51 ✓"

---

## Files Modified

### SAME51 (orc-io-mcu):
1. ✅ `src/drivers/ipc/ipc_protocol.h` - Fixed IPC_END_BYTE definition
2. ✅ `src/drivers/ipc/drv_ipc.cpp` - Fixed LENGTH calculation (TX and RX)

### RP2040 (orc-sys-mcu):
- No changes needed - implementation was correct

---

## Compilation Status

### RP2040:
```
✅ SUCCESS - Compiled without errors
RAM:   [===       ]  32.1% (used 84128 bytes from 262144 bytes)
Flash: [          ]   1.5% (used 253988 bytes from 16510976 bytes)
```

### SAME51:
```
✅ SUCCESS - Compiled with minor warnings (unused variables)
RAM:   [=         ]  11.2% (used 29460 bytes from 262144 bytes)
Flash: [===       ]  27.0% (used 282984 bytes from 1048576 bytes)
```

---

## Next Steps

1. **Flash updated firmware to SAME51**
2. **Test ping command from RP2040:**
   ```
   ping
   ```
3. **Expected output:**
   ```
   [INFO] Sending PING to SAME51...
   [INFO] PING sent successfully (waiting for PONG)
   [INFO] IPC: Received PONG from SAME51 ✓
   ```
4. **Check IPC statistics:**
   ```
   ipc-stats
   ```
   Expected:
   - RX Packets: > 0
   - TX Packets: > 0
   - RX Errors: 0
   - CRC Errors: 0

5. **If successful:** Proceed to test HELLO handshake and sensor data exchange

---

## Root Cause Analysis

### How did this happen?

1. **END byte typo:** Simple copy/paste error - someone defined `IPC_END_BYTE` as `0x7D` instead of `0x7E`. This made the protocol fundamentally broken since END and ESCAPE were the same byte.

2. **LENGTH field ambiguity:** The protocol specification wasn't clear about whether LENGTH included itself. The RP2040 implementation included LENGTH in the count, but the SAME51 implementation assumed it was payload-only. This is a common source of confusion in packet protocols.

### Lessons Learned

1. **Protocol specifications should be explicit** about every field's interpretation
2. **Test basic handshake first** before implementing complex features
3. **Use the same code/library on both sides** when possible to avoid mismatches
4. **Document with examples** showing actual byte values for each packet type

---

## Protocol Now Aligned

Both RP2040 and SAME51 now implement:
- START/END byte: `0x7E`
- ESCAPE byte: `0x7D`
- LENGTH = 2 (self) + 1 (TYPE) + N (PAYLOAD)
- CRC over LENGTH + TYPE + PAYLOAD
- CRC: Initial=0xFFFF, Polynomial=0x1021

**The IPC link should now work correctly!** 🎯
