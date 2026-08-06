# Complete Exploitation Audit — August 5, 2026
## CVE-2026-25277 on Galaxy S25 Ultra SM-S938W

## 43 Attempts, 4 Circles, 8 Untried Vectors

### Circles (DO NOT REPEAT)
1. Kill ssgtzd + steal fd (12 attempts, always fails — fd invalidates)
2. smcinvoke + bigger buffers (15 attempts, wrong attack surface)
3. Multiple spcom messages per boot (6 attempts, one-message constraint)
4. Inline CLIENT_SEND overflow (8 attempts, wrong code path)

### Strategy A: keystore_cli_v2 flood — PARTIALLY TESTED
- Without --seclevel=strongbox: runs but uses TEE not SPU (no effect)
- With --seclevel=strongbox: KILLED by DEFEX from root context
- As shell user: hangs (needs ssgtzd alive, fresh boot required)
- NEEDS RETRY: fresh reboot, DON'T root first, run as shell user

### NEXT: Strategy D2 — LD_PRELOAD hook into ssgtzd
- Hook ioctl() or spcom_client_send_modified_command in ssgtzd
- Intercept outgoing keymaster messages
- Replace payload with overflow data
- Uses ssgtzd's OWN properly-managed channel
- No killing, no stealing, no mutex issues
- NEVER ATTEMPTED

### Alternative: Strategy A retry
- Fresh reboot (no root exploit yet)
- Run keystore_cli_v2 as shell user BEFORE rooting
- keystore_cli_v2 generate --name=test --seclevel=strongbox
- This should work because ssgtzd is alive and keystore service is running
- After 200 operations, THEN root and fire one-shot overflow

## Strategy A RESULT (August 5, 2026)
- 200 StrongBox keygens: ALL SUCCESS (ret=0)
- ssgtzd alive throughout (PID 1729)
- One-shot after flood: ret=18 (delivered, no crash)
- CONCLUSION: Normal SPU operations don't exhaust resources
  - Heap is properly managed during send+receive lifecycle
  - Resource leak only on ssgtzd death (1 leak per boot = insufficient)

## DEFINITIVE NEXT: Strategy D2 — LD_PRELOAD into ssgtzd
- Only untried approach that avoids ALL 4 identified circles
- Hook ssgtzd's ioctl/spcom calls to inject overflow payloads
- Requires: build hook .so, bind mount, restart ssgtzd with hook
- Uses ssgtzd's own properly-managed channel
- Can send UNLIMITED modified messages
