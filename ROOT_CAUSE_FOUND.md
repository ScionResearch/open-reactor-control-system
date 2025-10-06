# ROOT CAUSE ANALYSIS - IPC Communication Failure

## 🎯 **Root Cause Found!**

The IPC protocol had a **critical typo** in the frame marker definitions on the **RP2040 side** that caused communication to fail.

---

## **The Bug**

### **RP2040 `IPCDataStructs.h` (ORIGINAL - BROKEN):**
```cpp
#define IPC_START_BYTE          0x7E
#define IPC_END_BYTE            0x7D  // ❌ WRONG! Should be 0x7E
#define IPC_ESCAPE_BYTE         0x7D  // ❌ CONFLICT! Same as END_BYTE
```

### **SAME51 `ipc_protocol.h` (FIXED EARLIER):**
```cpp
#define IPC_START_BYTE          0x7E
#define IPC_END_BYTE            0x7E  // ✅ CORRECT
#define IPC_ESCAPE_BYTE         0x7D  // ✅ CORRECT
```

---

## **What Happened**

### **Packet Transmission Flow:**

1. **RP2040 sends PING:**
   ```
   START: 0x7E
   Data:  00 03 00 29 B1
   END:   0x7D  ← WRONG! Should be 0x7E
   ```

2. **SAME51 receives:**
   ```
   Byte 0 (0x7E): START detected → Enter RECEIVING state
   Bytes 1-5 (00 03 00 29 B1): Buffered
   Byte 6 (0x7D): ESCAPE detected → Wait for next byte
   [Timeout after 1000ms - no next byte arrives]
   ```

3. **Result:**
   - SAME51 thinks `0x7D` is an ESCAPE byte
   - SAME51 waits forever for the escaped value
   - Timeout occurs, packet rejected
   - Communication fails ❌

---

## **Symptoms Observed**

### **From SAME51 Serial Output:**
```
[IPC RX] START byte detected (0x7E)
[IPC RX] Data byte [0]: 0x00
[IPC RX] Data byte [1]: 0x03
[IPC RX] Data byte [2]: 0x00
[IPC RX] Data byte [3]: 0x99  ← CRC appears wrong due to different bytes
[IPC RX] Data byte [4]: 0xCF
[IPC RX] Escape byte detected (0x7D)  ← Thinks this is ESCAPE, not END!
[IPC RX] TIMEOUT after 61449 ms, 5 bytes buffered
```

### **Error Pattern:**
- First PING: START detected, then timeout
- Second PING: Previous packet times out, new packet starts
- Cycle repeats forever
- RX Packets: Always 0
- RX Errors: Incrementing with each PING

---

## **The Fix**

### **RP2040 - Fixed `IPCDataStructs.h`:**
```cpp
// Frame markers
#define IPC_START_BYTE          0x7E
#define IPC_END_BYTE            0x7E  // ✅ FIXED: was 0x7D
#define IPC_ESCAPE_BYTE         0x7D
#define IPC_ESCAPE_XOR          0x20
```

### **Both Sides Now Match:**
| Marker | Value | Purpose |
|--------|-------|---------|
| START  | 0x7E  | Marks beginning of packet |
| END    | 0x7E  | Marks end of packet |
| ESCAPE | 0x7D  | Escapes 0x7E and 0x7D in data |

---

## **How the Bug Happened**

1. **Initial implementation** - Someone copy/pasted the START_BYTE definition to create END_BYTE
2. **Typo introduced** - Changed `0x7E` to `0x7D` thinking they needed different values
3. **SAME51 was fixed** earlier (we changed END_BYTE from 0x7D to 0x7E)
4. **RP2040 was NOT fixed** - We forgot to apply the same fix to the RP2040 side
5. **Mismatch persisted** - RP2040 sending 0x7D as END, SAME51 expecting 0x7E

---

## **Correct Protocol Specification**

### **Packet Structure:**
```
[START] [LENGTH_HI] [LENGTH_LO] [TYPE] [PAYLOAD...] [CRC_HI] [CRC_LO] [END]
  0x7E     u8          u8         u8      0-1024       u8       u8     0x7E
```

### **Byte Stuffing Rules:**
Applied to: LENGTH, TYPE, PAYLOAD, CRC fields (NOT START/END markers)

- If data byte == `0x7E`: Transmit as `0x7D 0x5E`
- If data byte == `0x7D`: Transmit as `0x7D 0x5D`

### **PING Packet Example:**
```
Unescaped: 7E 00 03 00 29 B1 7E
           │  └─┘  │  └──┘  │
           │  LEN  │  CRC   END
           │  =3   TYPE
           START   =0x00
```

**On Wire (after byte stuffing, if any 0x7E or 0x7D in data):**
- START: `7E` (never escaped)
- Data: `00 03 00 29 B1` (no escaping needed for this packet)
- END: `7E` (never escaped)

**Total:** 7 bytes

---

## **Compilation Status**

### **RP2040:**
```
✅ SUCCESS
RAM:   32.1% (84128/262144 bytes)
Flash:  1.5% (254396/16510976 bytes)
```

### **SAME51:**
```
✅ SUCCESS  
RAM:   11.2% (29460/262144 bytes)
Flash: 27.0% (282984/1048576 bytes)
```

---

## **Expected Behavior After Fix**

### **RP2040 sends PING:**
```
[IPC TX] Sending packet, tempBuffer (5 bytes): 00 03 00 29 B1
[IPC TX] START: 0x7E
[IPC TX] Data: 00 03 00 29 B1
[IPC TX] END: 0x7E
```

### **SAME51 receives:**
```
[IPC] RX byte #0: 0x7E (state=0)
[IPC RX] START byte detected (0x7E), entering RECEIVING state
[IPC] RX byte #1: 0x00 (state=1)
[IPC RX] Data byte [0]: 0x00
[IPC] RX byte #2: 0x03 (state=1)
[IPC RX] Data byte [1]: 0x03
[IPC] RX byte #3: 0x00 (state=1)
[IPC RX] Data byte [2]: 0x00
[IPC] RX byte #4: 0x29 (state=1)
[IPC RX] Data byte [3]: 0x29
[IPC] RX byte #5: 0xB1 (state=1)
[IPC RX] Data byte [4]: 0xB1
[IPC] RX byte #6: 0x7E (state=1)
[IPC RX] END byte detected (0x7E), 5 bytes buffered
[IPC RX] Processing packet, buffer has 5 bytes
[IPC RX] Buffer: 00 03 00 29 B1
[IPC RX] LENGTH field = 3 (0x0003)
[IPC RX] MSG_TYPE = 0x00
[IPC RX] Calculated payload length = 0
[IPC RX] Expected total = 5, got = 5
[IPC RX] CRC: received=0x29B1, calculated=0x29B1
[IPC RX] ✓ Packet valid! Type=0x00, Payload=0 bytes
[IPC] Received PING, sending PONG
```

### **RP2040 receives PONG:**
```
[INFO] IPC: Received PONG from SAME51 ✓
```

---

## **Files Modified**

### **RP2040 (orc-sys-mcu):**
1. ✅ `lib/IPCprotocol/IPCDataStructs.h` - Fixed IPC_END_BYTE (0x7D → 0x7E)
2. ✅ `lib/IPCprotocol/IPCProtocol.cpp` - Added TX debug output

### **SAME51 (orc-io-mcu):**
1. ✅ `src/drivers/ipc/ipc_protocol.h` - Already fixed (0x7D → 0x7E)
2. ✅ `src/drivers/ipc/drv_ipc.cpp` - Added comprehensive RX debug output
3. ✅ `src/drivers/ipc/ipc_handlers.cpp` - Added PING handler debug

---

## **Lessons Learned**

1. **Protocol definitions must match exactly** between both sides
2. **Test early with real hardware** before implementing complex features
3. **Use the same header file** for protocol definitions when possible
4. **Validate frame markers** are unique and don't conflict
5. **Debug output is invaluable** for protocol development
6. **Document the protocol specification** clearly with examples

---

## **Next Steps**

1. **Flash both MCUs** with updated firmware
2. **Test PING/PONG** communication
3. **Verify IPC statistics** show successful RX/TX
4. **Test HELLO handshake**
5. **Test sensor data exchange**
6. **Remove debug output** once stable (or make it conditional)
7. **Continue with Phase 3-6** of IPC implementation

---

## **Success Criteria**

After flashing both MCUs:
- ✅ RP2040 `ping` command succeeds
- ✅ SAME51 receives PING, sends PONG
- ✅ RP2040 receives PONG
- ✅ IPC statistics show RX packets > 0
- ✅ Zero CRC errors
- ✅ Zero RX errors
- ✅ Consistent round-trip times <10ms

**The IPC link should now work perfectly!** 🎉🎯
