# SPCOM Reverse Engineering Findings
## JackKnife Studios — August 4, 2026
## Based on disassembly of: libspcom.so, libspukeymintdeviceutils.so, spcom.ko, spu_service.mbn

---

### IOCTL Map (CONFIRMED from libspcom.so + spcom.ko)

| Ioctl | Dir | Size | Name | Confirmed By |
|-------|-----|------|------|-------------|
| 0xC0095301 | RW | 9 | GET_VERSION | kernel module |
| 0x402053E9 | W | 32 | REGISTER_CLIENT | libspcom.so disasm |
| 0x402053EA | W | 32 | REGISTER_SERVICE | libspcom.so 0x12628 |
| 0x402853ED | W | 40 | CLIENT_SEND | libspcom.so |
| 0x404853EE | W | 72 | SEND_MODIFIED (DMA) | libspcom.so 0x14844 |
| 0xC02853F0 | RW | 40 | GET_MESSAGE | libspcom.so 0x11ef4 |
| 0x402853F1 | W | 40 | LOCK_ION_BUFFER | libspcom.so 0x144c8 |
| 0x402853F2 | W | 40 | UNLOCK_ION_BUFFER | libspcom.so 0x144c8 |
| 0x402853F3 | W | 40 | SERVER_SEND_RESPONSE | libspcom.so 0x17008 |
| 0x402853F4 | W | 40 | SERVER_SEND_MODIFIED_RESP | libspcom.so |
| 0x80085302 | R | 8 | IS_CONNECTED | kernel module |

### Struct Formats (CONFIRMED from disassembly)

#### REGISTER_CLIENT / REGISTER_SERVICE (0xE9 / 0xEA)
```
Size: 32 bytes
[0-31]: channel_name (char[32], null-terminated)
```
Source: libspcom.so 0x144c8 — `strlcpy(struct, name, 32, 32)`

#### CLIENT_SEND (0xED) / SERVER_SEND_RESPONSE (0xF3)
```
Size: 40 + msg_len bytes
[0-31]:  channel_name (char[32])
[32-35]: timeout_ms (uint32)
[36-39]: tx_size (uint32) = msg_len
[40+]:   message_body (tx_size bytes)
```
Source: libspcom.so send functions

#### GET_MESSAGE (0xF0)
```
Size: EXACTLY 40 bytes (driver rejects larger buffers with EINVAL)
[0-31]:  channel_name (char[32])
[32-35]: timeout_ms (uint32)
[36-39]: max_rx_size (uint32)
Returns: message data overwrites struct, ret = bytes received
```
Source: Tested on phone — 40 bytes BLOCKS (correct), >40 bytes returns EINVAL

#### SEND_MODIFIED (0xEE) — DMA path
```
Size: 72 + msg_len bytes
[0-31]:  channel_name (char[32])
[32-35]: timeout_ms (uint32)
[36-39]: tx_size (uint32) = msg_len
[40-47]: ion_entry_0 = {fd:int32, msg_offset:uint32}
[48-55]: ion_entry_1 = {fd:int32, msg_offset:uint32}
[56-63]: ion_entry_2 = {fd:int32, msg_offset:uint32}
[64-71]: ion_entry_3 = {fd:int32, msg_offset:uint32}
[72+]:   message_body (tx_size bytes)
```
Source: libspcom.so 0x14844 — confirmed by malloc(size+72), strlcpy at 0, stp at 32, memcpy at 72, stur q0/q1 at 40/56

ION entry: fd = DMA buffer fd (-1 = unused), msg_offset = byte offset in message body
- msg_offset must be 8-byte aligned (tst x21, #7 at spcom.ko 0x58dc)
- msg_offset + 8 <= tx_size (sub x8, x20, #8; cmp at spcom.ko 0x590c)
- Driver writes 64-bit physical address (8 bytes) at msg[msg_offset]
- Source: spcom.ko modify_dma_buf_addr 0x5a10: `str x25, [x22, x21]`

#### LOCK_ION / UNLOCK_ION (0xF1 / 0xF2)
```
Size: 40 bytes
[0-31]:  channel_name (char[32])
[32-35]: ion_fd (int32) — DMA buffer fd to lock/unlock
[36-39]: padding (zero)
```
Source: libspcom.so 0x144c8 — `strlcpy(struct, name, 32)` then `str w21, [sp, #32]`

### SPU Firmware Message Format (from spu_service.mbn)

#### Command IDs
The SPU uses **4-byte ASCII command identifiers**, NOT integer command IDs.
Source: spu_service.mbn function 0xea0

Confirmed command IDs reaching vulnerable function 0xea0:
- **"TSID"** = bytes 0x54, 0x53, 0x49, 0x44 (uint32 0x44495354)
- **"STTA"** = bytes 0x53, 0x54, 0x54, 0x41 (uint32 0x41545453)

#### Vulnerable Function 0xea0 Stack Layout
```
Frame size: 0xb0 = 176 bytes
sp+0x00 to sp+0x27: local variables
sp+0x28 (40):       buffer start (data copied here)
sp+0x58 (88):       stack canary
sp+0x60 (96):       saved x29, x30 (frame pointer + return address)
sp+0x70 (112):      saved x26, x25
sp+0x80 (128):      saved x24, x23
sp+0x90 (144):      saved x22, x21
sp+0xa0 (160):      saved x20, x19
```
Canary offset from buffer: 88 - 40 = **48 bytes**

#### Overflow Mechanism (0xea0 error path)
1. Function receives (cmd_id, data_ptr, data_size, output_ptr)
2. Calls 0x17fc(context, data, size, output) — main processing
3. If 0x17fc FAILS (malformed data): enters error path
4. Error path calls 0x3b0 which copies data_size bytes to sp+40
5. NO upper bound check on data_size
6. If data_size > 48: overwrites canary at sp+88

### Keymaster Message Construction (from libspukeymintdeviceutils.so)

#### spuSetCmd (0xd050)
```c
// Internal buffer layout:
buf[108] = cmd_id;          // uint32_t — ASCII command like "TSID"
memcpy(buf+112, data, size); // payload data follows cmd
// Total: 4 + data_size bytes starting from offset 108
```

#### spu_tz_send_data (0xc620)
1. Allocates DMA buffer via DmabufHeapAlloc
2. Copies command data (from buf[108:]) to DMA buffer
3. DMA buffer contains: [cmd:4][payload:N]
4. Sends via spcom_client_send_modified_command with ION entries

### Test Results (Confirmed on Device)

| Test | Method | Result | Meaning |
|------|--------|--------|---------|
| cmd=0xFF (CLIENT_SEND) | Invalid ASCII | ret=256, immediate | SPU rejected, sent error response |
| cmd=0x02 (CLIENT_SEND) | Integer ID | ret=timeout | SPU accepted but hung (wrong format) |
| "TSID" (CLIENT_SEND) | Correct ASCII | ret=timeout | SPU accepted but hung (needs DMA data) |
| "STTA" (CLIENT_SEND) | Correct ASCII | ret=timeout | Same — needs DMA path |
| REGISTER_SERVICE (0xEA) | On phone | ret=0 | Service registration WORKS |
| GET_MESSAGE 40-byte | On phone | ETIMEDOUT (blocked) | Correct format, no client connected |
| GET_MESSAGE >40-byte | On phone | EINVAL | Driver rejects oversized buffer |

### Next Steps
- Use SEND_MODIFIED (0xEE) with DMA buffer containing overflow data
- Message body must reference DMA buffer at 8-byte aligned offset
- SPU will read data from DMA physical address
- Use qcom,sp-hlos DMA heap for SPU-accessible memory

---

## Session 11 Results (August 4, 2026)

### CONFIRMED: 18-byte Message Format Delivers to SPU

From `libspukeymintdeviceutils.so` function `sp_ext_msg_client_send_message` at 0xd954:

```
18-byte inline message:
[0-3]:  param (uint32) — e.g., 0x11 for cmd_index=2
[4-5]:  cmd_index (uint16) — dispatch table index (0-18)
[6-13]: ION placeholder (0xFFFFFFFFFFFFFFFF, replaced by phys addr)
[14-17]: dma_data_size (uint32) — bytes in DMA buffer

SEND_MODIFIED struct: 72 + 18 = 90 bytes total
ION entry: {dma_fd, offset=8} → replaces msg[8..15] with 64-bit physical address
```

**Test Result:** `ret=18` on fresh reboot = message delivered to SPU!
- param=0x11, cmd_index=2 → SPU dispatch accepts
- DMA buffer (sp-hlos heap) with TSID + overflow data accessible
- No crash detected — overflow path may not be triggered

### ssgtzd Binary Extracted
- Path: `/vendor/bin/ssgtzd` → `/home/jackknife/S25_Backup/strongbox_binaries/ssgtzd`
- Size: 184,320 bytes (extracted from /proc/PID/mem, bypassing DEFEX)
- SHA-256: `1155c0678d0c921b110a5b65b1984e9ac3d42b2fd2194604eab91c7a87b4b120`
- Uses MINK/QTEE framework, NOT direct spcom
- spcom communication handled by `libspukeymintdeviceutils.so` in keymaster HAL

### Spcom State Degradation
- After multiple kill/register cycles, spcom becomes unresponsive
- Need fresh reboot between test batches
- First test after reboot consistently works (ret=18)
- Subsequent tests on same channel timeout

### Next Steps
1. Disassemble spu_service.mbn function 0x17fc to understand error conditions
2. The overflow path in 0xea0 only executes when 0x17fc returns NON-ZERO
3. Need to find DMA data format that makes 0x17fc fail (triggering the copy-to-stack overflow)
4. Current TSID + 0xFF data may succeed at 0x17fc, bypassing the overflow path entirely

---

## SPCOM Channel State Machine (from spcom.ko full RE)

### Channel Structure (0x6F8 bytes per channel, 32 slots)
| Offset | Field | Purpose |
|--------|-------|---------|
| +0x00 | name[32] | Channel name |
| +0x20 | channel_lock | Mutex protecting state |
| +0x50 | tx_count | Transaction ID (init 0x12345678) |
| +0x98 | rpdev | rpmsg device pointer |
| +0xA8 | rx_done | Completion for response arrival |
| +0xE8 | **is_busy** | **THE BLOCKER: set on send, cleared by spcom_rx()** |
| +0xF0 | active_pid | PID owning current transaction |
| +0xF4 | ref_count | Open fd count |
| +0x140 | rx_buf_size | Response data size |
| +0x148 | rx_buf_ptr | Response data pointer |

### State Machine
```
IDLE (is_busy=0) → SEND → is_busy=1 → response arrives → rx_buf set
  → spcom_rx() called → copies response → clears rx_buf → IDLE
```
**Second send blocks because is_busy=1 is NEVER cleared without consuming response**

### Why read()/GET_MESSAGE Returns -1
- read() handler at 0xEB0 checks channel at file+216 (Samsung-specific offset)
- May require per-channel chardev created via ioctl 0xEB/0xEC
- GET_MESSAGE ioctl (0xC02853EE) may be for SERVER channels only
- spcom_device_write at 0x14D4 may be the proper SEND+WAIT path

### 13 Confirmed Ioctls
| Ioctl | Name |
|-------|------|
| 0xC0095301 | REGISTER_CLIENT/GET_VERSION |
| 0x80085302 | GET_NEXT_REQ_SIZE |
| 0xC01053E8 | HANDLE_SSR |
| 0x402053E9 | CREATE_CHANNEL (server) |
| 0x402053EA | CREATE_CHANNEL (client) |
| 0x402053EB | CREATE_CHANNEL + CHARDEV |
| 0x402053EC | CREATE_CHANNEL + CHARDEV (variant) |
| 0x402853ED | SEND_MESSAGE |
| 0xC02853EE | GET_MESSAGE (RW, 40 bytes) |
| 0x404853EE | SEND_MODIFIED (W, 72 bytes) |
| 0x402853F1 | LOCK_ION |
| 0x402853F2 | IS_CH_CONNECTED |
| 0x000053F3 | READ_FW_VERSION |

---

## write() Interface (from spcom_device_write RE)

### 7 Command Words via write()
| Value | ASCII | Purpose |
|-------|-------|---------|
| 0x53454E44 | SEND | Send message (fire-and-forget, no DMA) |
| 0x534E444D | SNDM | Send Modified (with DMA address patching) |
| 0x43524554 | CRTE | Create channel character device |
| 0x4C4F434B | LOCK | Lock DMA buffer by fd |
| 0x554C434B | ULCK | Unlock DMA buffer(s) |
| 0x45535352 | ESSR | Reset acknowledgment |
| 0x52535452 | RSTR | Restart SPU via rproc_boot |

### SEND Format (12+N bytes)
[cmd:4=0x53454E44][timeout:4][payload_size:4][payload:N]

### SNDM Format (44+N bytes)
[cmd:4=0x534E444D][dma0_fd:4][dma0_off:4][dma1_fd:4][dma1_off:4]
[dma2_fd:4][dma2_off:4][timeout:4][dma3_fd:4][dma3_off:4]
[payload_size:4][payload:N]

### KEY FINDING: write() is fire-and-forget!
- write() does NOT wait for SPU response
- write() does NOT call spcom_rx()
- write() acquires mutex, sends, releases — instantly reusable
- Responses accumulate at ch+0x140 via workqueue
- CRTE command WORKS (returned EBUSY = channel exists)
- SEND/SNDM/LOCK return EINVAL on main /dev/spcom fd
  - These commands need a CHANNEL-ASSOCIATED fd
  - Requires per-channel chardev or proper fd setup

### Blocker: fd Association
- write() SEND/SNDM needs the fd to be associated with a channel
- Main /dev/spcom fd doesn't have channel association for write()
- ioctl CREATE_CHANNEL (0xE9) sets up ioctl path but NOT write path
- Need to either:
  1. Create per-channel chardev and open it
  2. Use proper ioctl to set up write() channel association
  3. RE the exact fd->channel mapping in write handler

### No is_busy Flag!
Previous agent was WRONG about is_busy at ch+0xE8.
That field is an init flag, not a busy flag.
Channel serialization uses mutex at ch+0x20 only.
