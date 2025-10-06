# IPC Debug Output Guide - SAME51

## Debug Statements Added

I've added comprehensive debug output to the SAME51 IPC receiver to help diagnose the communication issues.

## What You'll See

### When PING is Received Successfully

```
[IPC RX] START byte detected (0x7E)
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

### When Errors Occur

#### Length Mismatch Error:
```
[IPC RX] START byte detected (0x7E)
[IPC RX] END byte detected (0x7E), X bytes buffered
[IPC RX] Processing packet, buffer has X bytes
[IPC RX] Buffer: XX XX XX ...
[IPC RX] LENGTH field = Y (0xYYYY)
[IPC RX] MSG_TYPE = 0xZZ
[IPC RX] Calculated payload length = N
[IPC RX] Expected total = A, got = B
[IPC RX] ERROR: IPC RX: Length mismatch (exp A, got B)
[IPC RX] Error state detected, clearing buffer and returning to IDLE
[IPC RX] Error message: IPC RX: Length mismatch (exp A, got B)
```

#### CRC Error:
```
[IPC RX] START byte detected (0x7E)
[IPC RX] END byte detected (0x7E), X bytes buffered
[IPC RX] Processing packet, buffer has X bytes
[IPC RX] Buffer: XX XX XX ...
[IPC RX] LENGTH field = Y (0xYYYY)
[IPC RX] MSG_TYPE = 0xZZ
[IPC RX] Calculated payload length = N
[IPC RX] Expected total = A, got = A
[IPC RX] CRC: received=0xAAAA, calculated=0xBBBB
[IPC RX] ERROR: IPC RX: CRC error (exp 0xBBBB, got 0xAAAA)
[IPC RX] Error state detected, clearing buffer and returning to IDLE
[IPC RX] Error message: IPC RX: CRC error (exp 0xBBBB, got 0xAAAA)
```

#### Packet Too Short:
```
[IPC RX] START byte detected (0x7E)
[IPC RX] END byte detected (0x7E), 3 bytes buffered
[IPC RX] Processing packet, buffer has 3 bytes
[IPC RX] Buffer: XX XX XX
[IPC RX] ERROR: Packet too short (3 bytes)
[IPC RX] Error state detected, clearing buffer and returning to IDLE
[IPC RX] Error message: IPC RX: Packet too short
```

## Debug Output Locations

### 1. **State Machine Events**
- **START byte detected** - When `0x7E` is received in IDLE state
- **END byte detected** - When `0x7E` is received in RECEIVING state (not escaped)

### 2. **Packet Processing**
- **Buffer contents** - First 32 bytes in hex format
- **LENGTH field** - Both decimal and hex
- **MSG_TYPE** - Hex value
- **Calculated payload length** - Based on LENGTH field
- **Expected vs actual total bytes** - Validation check
- **CRC comparison** - Received vs calculated

### 3. **Error Handling**
- **Error state detected** - When entering ERROR state
- **Error message** - The specific error that occurred

### 4. **Message Handlers**
- **PING received** - When PING message is successfully processed

## Interpreting the Output

### Scenario 1: No START Byte Detected
**Output:** Nothing (no debug messages)
**Meaning:** SAME51 is not receiving any data from RP2040
**Check:**
- UART TX/RX connections
- Baud rate (should be 1,000,000)
- Ground connection

### Scenario 2: START But No END
**Output:** 
```
[IPC RX] START byte detected (0x7E)
```
**Meaning:** START received but packet never completes
**Check:**
- Signal integrity (oscilloscope)
- END byte transmission from RP2040
- Timeout issues

### Scenario 3: START and END, But Length Mismatch
**Output:**
```
[IPC RX] START byte detected (0x7E)
[IPC RX] END byte detected (0x7E), X bytes buffered
[IPC RX] Processing packet, buffer has X bytes
[IPC RX] Buffer: 00 03 00 29 B1
[IPC RX] LENGTH field = 3 (0x0003)
[IPC RX] Expected total = 5, got = Y
[IPC RX] ERROR: Length mismatch
```
**Meaning:** Packet framing issue - bytes were dropped or added
**Check:**
- Byte stuffing implementation
- Buffer overflow
- UART timing

### Scenario 4: Correct Length, But CRC Fails
**Output:**
```
[IPC RX] CRC: received=0xAAAA, calculated=0xBBBB
[IPC RX] ERROR: CRC error
```
**Meaning:** Data corruption or CRC algorithm mismatch
**Check:**
- Compare buffer contents with expected packet
- Verify CRC algorithm matches on both sides
- Check for bit errors on UART line

### Scenario 5: Success!
**Output:**
```
[IPC RX] ✓ Packet valid! Type=0x00, Payload=0 bytes
[IPC] Received PING, sending PONG
```
**Meaning:** Everything works! 🎉

## Expected RP2040 PING Packet

When RP2040 sends PING, the buffer should show:
```
[IPC RX] Buffer: 00 03 00 29 B1
                 │  │  │  └──┘
                 │  │  │  CRC
                 │  │  MSG_TYPE (0x00 = PING)
                 │  LENGTH_LO (3)
                 LENGTH_HI (0)
```

**Breakdown:**
- `00 03` - LENGTH = 3 (includes 2 bytes of LENGTH field + 1 byte of TYPE)
- `00` - MSG_TYPE = 0x00 (IPC_MSG_PING)
- `29 B1` - CRC16-CCITT over bytes [00 03 00]

## Testing Procedure

1. **Flash updated SAME51 firmware**
2. **Open SAME51 serial monitor** (baud 115200)
3. **From RP2040, send:** `ping`
4. **Observe SAME51 debug output**
5. **Analyze the output** using the scenarios above

## Common Issues and Solutions

### Issue: Buffer shows wrong bytes
**Example:** `Buffer: 7E 00 03 00 29 B1`
**Problem:** START byte is in the buffer (shouldn't be)
**Solution:** START/END bytes should be stripped before buffering

### Issue: CRC always wrong but length correct
**Example:** Buffer correct, but CRC mismatch every time
**Problem:** CRC algorithm difference or byte order
**Solution:** Verify CRC implementation matches RP2040 exactly

### Issue: Length field interpretation
**Example:** LENGTH=3 but expecting 8 bytes total
**Problem:** Understanding what LENGTH includes
**Solution:** RP2040 and SAME51 must agree: LENGTH = 2 (self) + 1 (TYPE) + N (payload)

## Files Modified

**SAME51 (orc-io-mcu):**
1. ✅ `src/drivers/ipc/drv_ipc.cpp`
   - Added debug to `ipc_processRxByte()` (START/END detection)
   - Added debug to `ipc_processReceivedPacket()` (validation steps)
   - Added debug to `ipc_update()` (error state handling)

2. ✅ `src/drivers/ipc/ipc_handlers.cpp`
   - Added debug to `ipc_handle_ping()` (PING received)

## Compilation Status

```
✅ SUCCESS
RAM:   [=         ]  11.2% (used 29460 bytes from 262144 bytes)
Flash: [===       ]  27.0% (used 282984 bytes from 1048576 bytes)
```

## What to Report

When testing, capture and share:
1. Complete SAME51 serial output
2. The buffer contents hex dump
3. Any error messages
4. IPC statistics from both sides

This will help pinpoint the exact issue! 🔍
