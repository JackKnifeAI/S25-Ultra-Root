# CVE-2026-25277 Strongbox Exploit — Attack Plan

## Vulnerability
- **CVE-2026-25277**: Buffer Copy Without Checking Size (CWE-120)
- **CVSS**: 8.8 (High), UNPATCHED as of July 2026
- **Target**: Qualcomm Strongbox TA (spu_service.mbn) on Snapdragon 8 Elite

## Attack Surface (Mapped July 28, 2026)

### TEE Interface
- `/dev/smcinvoke` — SMC Invoke driver, 82 active refs
- `root_ioctl` — Main ioctl handler at 0xffffffe9dc4bbe14
- `process_invoke_req` — SMC call processor at 0xffffffe9dc4bbf4c
- `marshal_in_req` — Input marshaling at 0xffffffe9dc4bc3e0

### SPU Firmware
- `spu_service.mbn` — 90KB ELF ARM64 (the vulnerable TA)
- `spu_rpmb.mbn` — 41KB RPMB key management
- `libspukeymint.so` — 241KB marshaling library
- `libspukeymintdeviceutils.so` — 101KB utility library

### Vulnerable Functions (from spu_service.mbn strings)
- `AllocateAndCopyData` — THE vulnerable function
- `sp_keymint_attest_key` — Attestation key handling
- `km_provisioning_by_key` — Key provisioning
- `UsefulBuf_Copy` / `UsefulOutBuf_CopyOut` — QCBOR data copy

### Error Strings (buffer boundary indicators)
```
"buffer too small : data size %d!"
"[%s] VALUE_INPUT Size does not match with buffer [%d]"
"Response buffer is too short (%u < %u)"
"Integer Overflow: a=%u, b=%u"
"%s: Insufficient buff size filesize = 0x%x, buffer_len = 0x%x"
```

### Key Partitions (A/B)
- tz_a/b (sdd2/31) — TrustZone OS
- hyp_a/b (sdd17/43) — Hypervisor/RKP
- keymaster_a/b (sdd6/34) — KeyMaster TA
- tz_kg_a/b (sdd20/46) — TZ Key Generation
- tziccc_a/b (sdd18/44) — TZ ICC crypto

### DMA Heaps (TEE communication)
- `qcom,qseecom` — QSEE command buffer
- `qcom,qseecom-ta` — TA loading buffer
- `qcom,secure-sp-tz` — Secure Processor ↔ TZ
- `qcom,sp-hlos` — Secure Processor ↔ Linux

## Exploit Chain
```
Kernel Root (CVE-2026-43499)
    ↓
Open /dev/smcinvoke (root_open)
    ↓
Send oversized KeyMint request (root_ioctl → process_invoke_req)
    ↓
Buffer overflow in spu_service.mbn (AllocateAndCopyData)
    ↓
ROP chain in TEE context
    ↓
Strongbox master key extraction
    ↓
AVB key compromise → Permanent firmware modification
    ↓
RKP bypass → Persistent root across reboots
```

## Tools Built
- `strongbox_probe.c` — Phase 1 crash detection probe
- `spu_service.mbn` — Extracted for offline analysis (90KB ARM64 ELF)
- `libspukeymint.so` — Extracted for reverse engineering (241KB)

## Next Steps
1. Reverse engineer AllocateAndCopyData in spu_service.mbn
2. Determine exact buffer size boundary
3. Build ROP chain from spu_service.mbn gadgets
4. Craft controlled overflow payload
5. Extract Strongbox master key via shared memory exfiltration

JackKnife Studios, July 2026

## REVERSE ENGINEERING RESULTS (July 28, 2026)

### Vulnerable Code Path — IDENTIFIED

The buffer overflow is in the key data processing function at **vaddr 0xFD8-0x10B0** in `spu_service.mbn`:

```asm
; At 0xFD8: Function starts processing key request
fd8:  ldp  x26, x23, [sp, #0x8]     ; Load session params
ff8:  bl   0x3b0                     ; CALL: receive data header
ffc:  cbz  w0, 0x101c               ; On success → size check

; SIZE CHECK (the "buffer too small" guard)
101c: ldr  w2, [sp]                  ; Load data_size (32-bit!)
1020: cmp  x2, x21                   ; Compare with buffer_size (64-bit)
1024: b.ls 0x1040                    ; If data_size <= buffer_size → COPY

; ERROR PATH
1028: adrp x1, 0xa000               ; "buffer too small : data size %d!"
1030: add  x1, x1, #0xeb4

; COPY PATH (potentially vulnerable)
1040: add  x8, sp, #0x28            ; Destination: STACK buffer at sp+0x28
1044: str  x2, [sp, #0x28]          ; Store size at destination
1048: mov  w9, #0xc                  ; Type = 12
104c: mov  w3, #0x11                ; Param = 17
1068: bl   0x3b0                     ; CALL: receive actual data → OVERFLOW HERE
```

### AllocateAndCopyData Function at 0x6A44

```asm
6a44: bti  c                        ; Branch Target ID (CFI)
6a48: pacib x30, sp                 ; PAC: Pointer Authentication
6a4c: sub  sp, sp, #0x50            ; 80-byte stack frame
6a6c: ldr  x8, [x0, #0x18]         ; Size from input (user-controlled)
6a74: cmp  x8, #0x4                 ; Minimum 4 bytes
6a78: b.lo error
6a7c: ldr  x9, [x0]                ; Source buffer
6a84: ldr  x10, [x0, #0x10]        ; Offset
6a94: add  x0, x9, x10             ; Effective source = buffer + offset
6a98: bl   0x3ffc                   ; Copy 4 bytes (header read)
```

### Security Mitigations Present

1. **Stack Canary**: At `[x29, #-0x8]`, loaded from `[0xf000+0x100]`
2. **PAC (Pointer Authentication)**: `pacib/retab` on function entry/exit
3. **BTI (Branch Target Identification)**: `bti c` at function entries
4. **Size Check**: `cmp x2, x21` at 0x1020 (but may be bypassable)

### Overflow Vector

The vulnerability is in the function at 0x3b0 (data transfer function):
- Called at 0x1068 after the size check passes
- Receives data from smcinvoke shared buffer
- Copies to stack buffer at sp+0x28
- The size check at 0x1020 uses 32-bit load (`ldr w2`) vs 64-bit compare (`cmp x2`)
- Integer truncation: if wire size has bits 32-63 set, w2 truncates to small value
- The actual transfer function may copy based on wire length, not declared size

### Key Strings in SPU TA Binary

```
"AllocateAndCopyData"
"buffer too small : data size %d!"
"[%s] VALUE_INPUT Size does not match with buffer [%d]"
"Response buffer is too short (%u < %u)"
"Integer Overflow: a=%u, b=%u"
"sp_keymint_attest_key"
"km_provisioning_by_key"
"UsefulBuf_Copy" / "UsefulOutBuf_CopyOut"
```

### Dependencies

- `libcmnlib.so` — Common TZ library (qsee_malloc, etc.)
- QCBOR encoder/decoder for data marshaling
- UsefulBuf library for buffer management
