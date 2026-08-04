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
