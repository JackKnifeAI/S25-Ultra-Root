# StrongBox Canary Siege — Progress Tally
## CVE-2026-25277 Stack Canary Brute-Force
## JackKnife Studios — July 2026

---

### Operation Parameters
- **Target:** Samsung StrongBox SPU stack canary (8 bytes)
- **Method:** spcom kill→register→SEND overflow, 1 probe per reboot
- **Cycle time:** ~53 seconds (reboot + root + probe + capture)
- **Total probes needed:** 2048 (256 guesses × 8 bytes)
- **Estimated completion:** ~28 hours from launch
- **Controller PID:** 1296521
- **Log:** ~/canary_brute.log
- **Results:** ~/S25_Backup/crypto_analysis/canary_results_all.txt
- **Phone state:** /data/local/tmp/canary_params.txt

### Monitoring Schedule
- Check every 3.69 hours (3h 41m 24s)
- Auto-respond to failures
- Commit progress at each check

---

### Session 10 Code Commits (17 total)

| # | Hash | Description |
|---|------|-------------|
| 1 | ca432fb | Session 10: Canary oracle + ECDSA lattice tools |
| 2 | c68b5ee | spcom_canary_oracle.c: Proven spcom path, SEND=256 confirmed |
| 3 | e524257 | BREAKTHROUGH: Integer truncation is the real overflow vector |
| 4 | 859185b | int_truncation_overflow.c: CBOR parser rejects truncation sizes |
| 5 | 492f898 | MAPPED: All 19 SPU commands, cmd 0x02 and 0x03 reach vulnerable function |
| 6 | 137eb84 | PROTOCOL DECODED: SPU is CLIENT, we are SERVER |
| 7 | 6da4d75 | triggered_overflow.c: TWO-THREAD ATTACK, trigger mechanism WORKS |
| 8 | 4a497ad | SMCINVOKE OP=2 COUNTS=0x1100 → result=0 (SPU PROCESSED!) |
| 9 | 87fb7e5 | smci_canary_crack.c: Canary brute-force via op=2 counts=0x1100 |
| 10 | 5198c5c | ANALYSIS: smcinvoke op=2 is SAFE — overflow is spcom RESPONSE path ONLY |
| 11 | a336b2b | spcom_mitm_canary.c: RECV+SEND protocol MITM for canary extraction |
| 12 | 6cab5a9 | FOUND: Correct RECV ioctl is GET_MESSAGE = 0xC02853F0 |
| 13 | a015dca | PROTOCOL CLARIFIED: WE SEND to SPU, SPU copies our data to stack |
| 14 | d7241fe | canary_batch.c + canary_brute_controller.sh: Automated brute-force |
| 15 | 8a15525 | BATCH 1 RESULT: Only 1 probe per boot works (ret=256 on first only) |
| 16 | 1b32555 | canary_batch.c: 1 probe per boot + run_canary_brute.sh controller |
| 17 | 82d9492 | BRUTE-FORCE RUNNING — 53s/cycle verified, controller autonomous |

---

### Siege Progress Log (append only)

#### Launch — 2026-07-29 23:19 PDT
- Controller started PID 1296521
- Cycles verified: 3 complete, 53s/cycle
- All probes: ret=256 (delivered), crash=0
- Status: RUNNING


#### Check 1 — 2026-07-30 03:41 PDT (~4.3 hours in)
- **Cycles completed:** 258
- **Byte 0 progress:** 255/256 (99%) — nearly complete
- **All probes:** ret=256 (delivered to SPU)
- **Crashes:** 0 (no canary byte differentiation yet)
- **Timing range:** 0.194ms — 3.871ms (avg 1.236ms)
- **Controller:** RUNNING (PID 1296521)
- **Rate:** ~57s per cycle (258 cycles in 4.3 hours)
- **Status:** NOMINAL — byte 0 almost done, no crashes detected
- **NOTE:** Zero crashes across 255 guesses means either:
  (a) The overflow doesn't reach the canary through this pending request path
  (b) The canary is checked on a different code path than the one processing our data
  (c) The pending request handling doesn't trigger the vulnerable function
  Will analyze full dataset after all 2048 probes complete per orders.

#### Check 2 — 2026-07-30 07:08 PDT (~7.8 hours in)
- **Cycles completed:** 395
- **Byte 0:** 256/256 (100% COMPLETE)
- **Byte 1:** 131/256 (51%)
- **All probes:** ret=256 (all delivered)
- **Crashes:** 0
- **Controller:** RUNNING (PID 1296521)
- **Rate:** ~57s/cycle steady
- **ETA remaining:** ~26h 17m (1661 probes left)
- **Status:** NOMINAL — phone mid-reboot at check time (adb: no devices)
- **NOTE:** Byte 0 completed with zero crashes across all 256 guesses.
  Byte 1 half done, also zero crashes. Pattern holding.
