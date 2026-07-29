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
