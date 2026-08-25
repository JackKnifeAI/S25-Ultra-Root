# StrongBox Diagnostic Probe Results
## CVE-2026-25277 — Samsung S25 Ultra SPU Attack
## JackKnife Studios — July 30, 2026

---

## Diagnostic 01: Does Pending SEND Crash SPU?

**Question:** When we SEND 256 bytes of 0xFF via the pending request
path (kill ssgtzd → register → SEND), does the SPU crash?

**Result:** NO CRASH
- SEND ret=256 (delivered in 2.167ms)
- ssgtzd PID unchanged (1704→1704)
- SPU processed the data without dying

**Conclusion:** The pending request after killing ssgtzd does NOT
route through the vulnerable function 0xea0. The data is handled
by a safe code path.

---

## Diagnostic 02: Does Triggered SEND Crash SPU?

**Question:** When we trigger an active StrongBox keystore operation
while our SEND is queued, does the SPU crash?

**Result:** NO CRASH
- SEND ret=256 (delivered, sender completed normally!)
- ssgtzd PID unchanged (1709→1709)
- The triggered path DOES cause the SPU to use our channel
- But 256 bytes of 0xFF still doesn't crash it

**Conclusion:** The SPU's copy function enforces an internal size
limit. It reads a SIZE from the response header and only copies
that many bytes. Raw 0xFF has no valid size header → copies 0 or
minimal bytes → canary never reached.

**Key Discovery:** The triggered approach works without D-state!
The sender completes normally. This is reliable multi-probe per boot.

---

## Diagnostic 03: What Does SPU Response Format Look Like?

**Question:** Can we capture the real response format that the SPU
expects from the keymaster server?

**Result:** No response data captured
- smcinvoke op=2 counts=0x1100: result=0 but zero output bytes
- All other counts values: rejected by smcinvoke driver (result=0xBEEF)
- Cannot steal ssgtzd fds from LD_PRELOAD context
- The smcinvoke path does not expose response data in DMA buffers

**Conclusion:** The SPU response format must be reverse-engineered
from the spu_service.mbn binary itself. The response parser at 0xfd8
reads a header first (via bl 0x3b0 at 0xff8), then uses the header's
size field to determine how much data to copy.

---

## Aggregated Analysis

### What We Know For Certain:
1. spcom SEND delivers 256 bytes to SPU (ret=256)
2. Triggered path works reliably (sender completes, no D-state)
3. Raw data does NOT crash SPU — response format matters
4. smcinvoke op=2 counts=0x1100 reaches SPU (result=0)
5. The SPU response parser reads a HEADER before copying data
6. Canary is at byte 48 from buffer start (verified from disassembly)

### What The Response Header Must Contain:
From the disassembly at 0xfd8-0x101c:
- First call to 0x3b0 at 0xff8: reads 16 bytes (w1=2, w3=0x10)
  This is the HEADER read. w3=0x10=16 bytes of header.
- Header contains a SIZE field at [sp] (loaded by ldr w2,[sp] at 0x101c)
- If SIZE > x21 (buffer limit): error "buffer too small"
- If SIZE <= x21: proceeds to copy SIZE bytes to sp+0x28

### To Trigger Overflow:
The response must have a valid header where the SIZE field passes
the check (SIZE <= x21) but then the actual copy (at 0x1068) copies
MORE than SIZE bytes. This is the CVE-2026-25277 mismatch.

The header format is determined by the function pointer in x26
(loaded from [sp+8] at 0xfd8). This function is the spcom message
receive callback — it parses the spcom wire format.

### Next Step:
Reverse-engineer the 16-byte header format from:
1. The first bl 0x3b0 call parameters (w1=2, w3=0x10, dest=x29-0x28)
2. The spcom wire protocol documentation (if any)
3. The spu_kill_recv.c session 14 results (it captured 6+ responses)
4. The killbomb.txt output which shows ret=256 on first SEND

The header likely contains: [message_type:u32][data_size:u32][...padding...]
We need data_size to be SMALL (passes check) but the wire data to be LARGE.
