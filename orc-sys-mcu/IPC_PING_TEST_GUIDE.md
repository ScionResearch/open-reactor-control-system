# IPC Ping Test Guide - RP2040 Side

## Overview
This guide explains how to test the basic IPC communication between the RP2040 and SAME51 using the ping functionality.

## Prerequisites

1. **Hardware Setup:**
   - RP2040 (orc-sys-mcu) connected to Serial1 (UART)
   - SAME51 (orc-io-mcu) connected to Serial1 (UART)
   - Cross-connected TX/RX lines between the two MCUs
   - Common ground connection
   - Both MCUs powered and running

2. **Serial Monitor:**
   - Connect to RP2040 USB serial port
   - Baud rate: 115200
   - Line ending: Newline (NL)

## New Terminal Commands

### 1. `ping` - Send PING to SAME51
Sends a PING message to the SAME51 and waits for a PONG response.

**Usage:**
```
ping
```

**Expected Output:**
```
Sending PING to SAME51...
PING sent successfully (waiting for PONG)
IPC: Received PONG from SAME51 ✓
```

**If PONG not received:**
- Check UART connections (TX/RX crossed)
- Check baud rate (should be 1 Mbps on both sides)
- Check ground connection
- Run `ipc-stats` to see error counts

### 2. `ipc-stats` - Display IPC Statistics
Shows detailed statistics about IPC communication.

**Usage:**
```
ipc-stats
```

**Expected Output:**
```
=== IPC Statistics ===
RX Packets: 5
TX Packets: 3
RX Errors: 0
CRC Errors: 0
Last RX: 1250 ms ago
Last TX: 2300 ms ago
```

**Statistics Meaning:**
- **RX Packets:** Total packets received successfully
- **TX Packets:** Total packets transmitted
- **RX Errors:** Packet framing or buffer overflow errors
- **CRC Errors:** Packets that failed CRC validation
- **Last RX/TX:** Time since last successful communication

## Testing Procedure

### Step 1: Verify Initial State
```
ipc-stats
```

Expected initial state:
- RX Packets: 0 (or small number from HELLO handshake)
- TX Packets: 0 (or 1 from HELLO message)
- RX Errors: 0
- CRC Errors: 0

### Step 2: Send First Ping
```
ping
```

**What happens:**
1. RP2040 sends PING packet (start: 0x7E, msg type: 0x00, end: 0x7E)
2. SAME51 receives PING
3. SAME51 sends PONG response (msg type: 0x01)
4. RP2040 receives PONG and logs confirmation

**Success Criteria:**
- "PING sent successfully" message appears
- "Received PONG from SAME51 ✓" message appears within ~10ms
- No error messages

### Step 3: Verify Statistics Updated
```
ipc-stats
```

Expected after successful ping:
- RX Packets: +1 (PONG received)
- TX Packets: +1 (PING sent)
- RX Errors: 0
- CRC Errors: 0
- Last RX: <10 ms ago
- Last TX: <10 ms ago

### Step 4: Repeat Ping Test
Send multiple pings to verify consistent operation:
```
ping
ping
ping
```

Each should respond with PONG.

### Step 5: Continuous Monitoring
Check statistics periodically:
```
ipc-stats
```

Monitor for:
- Increasing packet counts
- Zero error counts
- Recent RX/TX timestamps

## Troubleshooting

### Problem: "Failed to send PING (TX queue full)"

**Cause:** TX queue is full (8 packets max)

**Solution:**
1. Wait a moment for queue to drain
2. Check if `ipc.update()` is being called regularly
3. Verify SAME51 is running and processing messages

### Problem: PING sent but no PONG received

**Possible Causes:**

1. **UART Connection Issues:**
   - TX/RX lines not crossed
   - Loose connection
   - Wrong baud rate

2. **SAME51 Not Responding:**
   - SAME51 not running
   - SAME51 IPC driver not initialized
   - SAME51 PING handler not registered

3. **Electrical Issues:**
   - Voltage level mismatch
   - No common ground
   - Signal interference

**Debugging Steps:**
```
ipc-stats
```

Check for:
- **High RX Errors:** Framing issues, wrong baud rate
- **High CRC Errors:** Noise, signal integrity issues
- **TX Packets but zero RX:** SAME51 not responding or wrong RX pin

### Problem: CRC Errors

**Symptoms:** 
```
CRC Errors: 5
```

**Possible Causes:**
- Signal integrity issues (long wires, no ground)
- Noise on the UART lines
- Baud rate mismatch
- Electrical interference

**Solutions:**
1. Shorten UART cables
2. Add ground wire if missing
3. Check baud rate on both sides (must be 1 Mbps)
4. Move away from noise sources (motors, relays)
5. Add series resistors (10-100Ω) on TX lines

### Problem: RX Errors

**Symptoms:**
```
RX Errors: 3
```

**Possible Causes:**
- Buffer overflow
- Invalid packet format
- State machine stuck

**Solutions:**
1. Ensure `ipc.update()` called frequently (every 5-10ms)
2. Check for blocking code that prevents IPC processing
3. Verify packet structure on SAME51 side
4. Power cycle both MCUs to reset state machines

## Protocol Details

### PING Packet Structure
```
[0x7E] [0x00][0x03] [0x00] [CRC_HI][CRC_LO] [0x7E]
  ^      ^     ^      ^       ^               ^
  |      |     |      |       |               |
START  LENGTH  TYPE  (no    CRC16          END
       (3 bytes)     payload)
```

### PONG Packet Structure
```
[0x7E] [0x00][0x03] [0x01] [CRC_HI][CRC_LO] [0x7E]
  ^      ^     ^      ^       ^               ^
  |      |     |      |       |               |
START  LENGTH  TYPE  (no    CRC16          END
       (3 bytes)     payload)
```

### Byte Stuffing
If any byte in LENGTH, TYPE, PAYLOAD, or CRC equals 0x7E or 0x7D, it is escaped:
- Original: 0x7E → Transmitted: 0x7D 0x5E
- Original: 0x7D → Transmitted: 0x7D 0x5D

## Success Metrics

A successful ping test should show:
- ✅ PONG received within 10ms
- ✅ Zero CRC errors
- ✅ Zero RX errors
- ✅ Consistent round-trip times
- ✅ 100% response rate (all PINGs get PONGs)

## Next Steps After Successful Ping Test

Once ping/pong is working reliably:

1. **Test HELLO Handshake:**
   - Verify HELLO message exchange on boot
   - Check protocol version compatibility

2. **Test Sensor Data:**
   - Send test sensor data from SAME51
   - Verify MQTT publishing on RP2040

3. **Test Index Sync:**
   - Request object index from SAME51
   - Verify index data received correctly

4. **Stress Test:**
   - Send rapid-fire pings
   - Monitor for dropped packets
   - Verify TX queue handling

5. **Long-term Reliability:**
   - Run ping every second for 1 hour
   - Monitor for communication degradation
   - Check for memory leaks or buffer issues

## Additional Commands

### Other Available Commands:
- `status` - Print system status
- `ip` - Print network configuration  
- `sd` - Print SD card info
- `ipc-test temp 25.5` - Simulate temperature sensor (for MQTT testing)
- `reboot` - Restart RP2040

## Monitoring Tips

1. **Keep stats visible:** Run `ipc-stats` frequently to monitor health

2. **Watch for patterns:** Regular TX but no RX means one-way communication

3. **Compare both sides:** If available, check SAME51 stats too

4. **Log errors:** Note any CRC or RX errors for pattern analysis

5. **Timing matters:** If PONG takes >100ms, investigate latency

## Hardware Connection Reference

**RP2040 Serial1 (Default pins may vary by board):**
- TX: GPIO 8 (or as defined in PIN_SI_TX)
- RX: GPIO 9 (or as defined in PIN_SI_RX)

**SAME51 Serial1:**
- TX: Connected to RP2040 RX
- RX: Connected to RP2040 TX
- GND: Common ground required

**Baud Rate:** 1,000,000 bps (1 Mbps)
**Format:** 8N1 (8 data bits, no parity, 1 stop bit)

## Notes

- The IPC protocol runs at **1 Mbps** for high-speed communication
- Each ping/pong is ~8 bytes, taking ~80μs to transmit
- Round-trip latency should be <10ms including processing time
- The TX queue can hold 8 packets, sufficient for burst traffic
- CRC16-CCITT ensures data integrity over the UART link
- Byte stuffing handles special characters (0x7E, 0x7D) in data

## Support

If ping tests consistently fail:
1. Document exact error messages
2. Capture `ipc-stats` output
3. Note hardware setup (board types, connections)
4. Check firmware versions on both MCUs
5. Review `IPC_MIGRATION_SUMMARY.md` for protocol details
