# IPC Protocol Debugging Guide

## Current Issue: SAME51 Incrementing Error Count

You're seeing:
- RP2040 sends PING successfully
- SAME51 receives data (errors increment)
- SAME51 does NOT respond with PONG
- Signals look clean on oscilloscope
- TX/RX lines correctly connected

This indicates a **protocol interpretation mismatch** - the SAME51 is rejecting packets at the protocol layer.

## New Debug Commands Added

### 1. `ping-raw` - Send Raw PING Packet
Sends a manually constructed PING packet with visible hex output.

**Usage:**
```
ping-raw
```

**Expected Output:**
```
Sending raw PING bytes to Serial1...
Raw PING: 7E 00 03 00 29 B1 7E
Raw PING sent
```

This shows the exact bytes being transmitted:
- `7E` - START byte
- `00 03` - LENGTH (3 bytes = message type only, no payload)
- `00` - Message Type (IPC_MSG_PING)
- `29 B1` - CRC16-CCITT checksum
- `7E` - END byte

### 2. `ipc-dump` - Dump Raw Bytes
Reads and displays raw bytes from Serial1 for 2 seconds.

**Usage:**
```
ipc-dump
```

**Purpose:**
- See what the SAME51 is actually sending
- Compare packet format between RP2040 and SAME51
- Identify format differences

## Common Protocol Mismatch Issues

### Issue 1: Different CRC Initialization

**RP2040 Current Implementation:**
```c
uint16_t crc = 0xFFFF;  // Initial value
// ... CRC calculation ...
// Polynomial: 0x1021
```

**Check SAME51:**
- Does it use `0xFFFF` or `0x0000` initial value?
- Does it use polynomial `0x1021`?
- Does it calculate over LENGTH + TYPE + PAYLOAD?

### Issue 2: Byte Order (Endianness)

**RP2040 sends LENGTH as:**
```
[LENGTH_HI][LENGTH_LO]
```

**Check SAME51:**
- Does it expect `[LENGTH_LO][LENGTH_HI]` instead?
- Are multi-byte fields (CRC, length) swapped?

### Issue 3: What's Included in CRC

**RP2040 CRC covers:**
```
CRC = calculateCRC16([LENGTH_HI, LENGTH_LO, TYPE, ...PAYLOAD])
```

**Check SAME51:**
- Does it include START/END bytes in CRC? (it shouldn't)
- Does it include LENGTH field in CRC?
- Does it calculate CRC differently?

### Issue 4: Length Field Interpretation

**RP2040 LENGTH = 3 for PING means:**
- Total bytes in: LENGTH(2) + TYPE(1) + PAYLOAD(0)
- So LENGTH = 3 = size of (LENGTH field + TYPE + PAYLOAD)

**Alternative interpretations:**
- LENGTH = payload only (would be 0 for PING)
- LENGTH = TYPE + PAYLOAD (would be 1 for PING)
- LENGTH = entire packet including CRC

### Issue 5: Byte Stuffing

**RP2040 escapes:**
- `0x7E` → `0x7D 0x5E`
- `0x7D` → `0x7D 0x5D`

**Applied to:** LENGTH, TYPE, PAYLOAD, CRC (NOT start/end bytes)

**Check SAME51:**
- Does it apply byte stuffing?
- Does it apply it to the same fields?
- Does it use the same escape sequences?

## Debugging Steps

### Step 1: Capture Raw Packet from RP2040

Run `ping-raw` command and note the exact bytes:
```
ping-raw
```

Expected output example:
```
Raw PING: 7E 00 03 00 29 B1 7E
```

### Step 2: Capture Raw Packets from SAME51

Run `ipc-dump` command:
```
ipc-dump
```

This will show what the SAME51 is sending. Compare:
- Packet structure
- Byte order
- CRC values
- Escape sequences

### Step 3: Manual CRC Verification

Calculate CRC manually for the PING packet:

**Input to CRC:** `00 03 00` (LENGTH_HI, LENGTH_LO, TYPE)

**CRC16-CCITT Algorithm:**
```
Initial: 0xFFFF
Polynomial: 0x1021

Byte 0x00:
  crc = 0xFFFF ^ (0x00 << 8) = 0xFFFF
  ... 8 iterations of shifting with conditional XOR ...
  
Byte 0x03:
  ... continue ...
  
Byte 0x00:
  ... continue ...

Final CRC should be: 0x29B1
```

If the SAME51 expects a different CRC, this is the issue.

### Step 4: Check SAME51 Baud Rate

Verify SAME51 UART is configured for **1,000,000 baud** (1 Mbps).

Common mismatches:
- 115200 (too slow, would show garbled data)
- 921600 (close but wrong)
- 2000000 (2 Mbps, too fast)

### Step 5: Check SAME51 Code

Look for the SAME51 IPC initialization:
```cpp
// What baud rate?
Serial1.begin(??????);

// What CRC init value?
uint16_t crc = 0x???? or 0x0000;

// What polynomial?
polynomial = 0x???? or 0x1021;

// What's included in CRC?
calculateCRC16([length, type, payload]);  // RP2040 way
calculateCRC16([type, payload]);          // Alternative
calculateCRC16([payload]);                // Another alternative
```

## Protocol Specification Reference

### RP2040 Packet Format

```
[START] [LEN_HI] [LEN_LO] [TYPE] [PAYLOAD...] [CRC_HI] [CRC_LO] [END]
  0x7E    u8       u8       u8      0-512 bytes   u8       u8     0x7E
```

### Field Definitions

- **START/END:** Always `0x7E`, never escaped
- **LENGTH:** 16-bit, big-endian, includes LENGTH(2) + TYPE(1) + PAYLOAD(N) bytes
- **TYPE:** 8-bit message type (0x00 = PING, 0x01 = PONG)
- **PAYLOAD:** 0-512 bytes of message data
- **CRC:** CRC16-CCITT over LENGTH + TYPE + PAYLOAD

### Byte Stuffing Rules

Applied to: LENGTH, TYPE, PAYLOAD, CRC fields

- If byte = `0x7E`: Replace with `0x7D 0x5E`
- If byte = `0x7D`: Replace with `0x7D 0x5D`

NOT applied to START/END bytes.

### CRC16-CCITT Algorithm

```c
uint16_t calculateCRC16(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;  // Initial value
    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;  // Polynomial
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}
```

**Parameters:**
- Initial value: `0xFFFF`
- Polynomial: `0x1021`
- Input: LENGTH(2) + TYPE(1) + PAYLOAD(N)
- Output: 16-bit checksum, big-endian

## Example Packets

### PING Packet (No Byte Stuffing Needed)
```
7E 00 03 00 29 B1 7E
│  │  │  │  │  │  └─ END
│  │  │  │  │  └──── CRC_LO
│  │  │  │  └─────── CRC_HI
│  │  │  └────────── TYPE (0x00 = PING)
│  │  └───────────── LENGTH_LO (3 bytes)
│  └──────────────── LENGTH_HI
└─────────────────── START
```

### PONG Packet
```
7E 00 03 01 28 21 7E
│  │  │  │  │  │  └─ END
│  │  │  │  │  └──── CRC_LO
│  │  │  │  └─────── CRC_HI
│  │  │  └────────── TYPE (0x01 = PONG)
│  │  └───────────── LENGTH_LO
│  └──────────────── LENGTH_HI
└─────────────────── START
```

### Packet with Byte Stuffing
If CRC_HI happened to be `0x7E`:
```
7E 00 03 00 7D 5E B1 7E
                └──┘
              Escaped 0x7E
```

## Likely Root Causes

Based on "data received but not interpreted":

1. **CRC Mismatch (Most Likely)** - 80% probability
   - Different initial value
   - Different polynomial
   - Different fields included in CRC
   - Byte order in CRC

2. **Length Field Interpretation** - 15% probability
   - SAME51 expects different LENGTH definition
   - Length doesn't include what RP2040 thinks

3. **Byte Stuffing Mismatch** - 3% probability
   - One side does it, other doesn't
   - Different escape sequences

4. **Baud Rate Close But Wrong** - 2% probability
   - 1 Mbps vs 1.5 Mbps or similar
   - Would cause some bits to decode correctly

## Recommended Debugging Sequence

1. **Run `ping-raw`** - Get exact bytes sent by RP2040
2. **Run `ipc-dump`** - Get exact bytes sent by SAME51
3. **Compare packet structures** - Look for obvious differences
4. **Check SAME51 source code** - Verify CRC algorithm matches
5. **Try alternative CRC settings** - If code matches but still fails
6. **Check for byte order differences** - Swap endianness if needed

## Quick Test: Alternative CRC Init Value

If you suspect CRC init value is the issue, you can temporarily test by modifying the RP2040 to use `0x0000` instead of `0xFFFF`:

In `IPCProtocol.cpp`, line 27, change:
```cpp
// OLD:
uint16_t crc = 0xFFFF;

// TEST:
uint16_t crc = 0x0000;
```

Recompile and try `ping` again. If it works, you've found the issue.

## Next Steps

Once you identify the mismatch:
1. Decide which side should be "correct"
2. Update the other side to match
3. Document the agreed protocol specification
4. Ensure both codebases use the same constants/algorithms

## Contact Info

If you need help interpreting the debug output, capture:
1. Output from `ping-raw`
2. Output from `ipc-dump`
3. SAME51 error counter behavior
4. Any relevant SAME51 source code snippets

This will help pinpoint the exact mismatch.
