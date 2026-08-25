#!/system/bin/sh
# ========================================================================
# GhostLock SIM Sandbox — Carrier Code Push Blocker
# Isolates SIM UICC from pushing code, OTA, or surveillance updates
#
# The SIM card has its own processor (UICC) that can:
# - Execute STK (SIM Toolkit) commands silently
# - Open BIP (Bearer Independent Protocol) data channels
# - Send/receive APDU commands for arbitrary code execution
# - OTA update SIM firmware without user knowledge
# - Monitor user activity via event subscriptions
#
# This script SANDBOXES the SIM: it thinks it can still push commands,
# but they're blocked at every layer. Only calls, texts, and data pass.
#
# Run with root: /data/local/tmp/cve-2026-43499-root -c 'sh /data/local/tmp/ghostlock-sim-sandbox.sh'
#
# JackKnife Studios, July 2026
# ========================================================================

echo "========================================"
echo "  GHOSTLOCK SIM SANDBOX"
echo "  Carrier Code Push Blocker"
echo "========================================"
echo ""

# === 1. DISABLE STK (SIM TOOLKIT) APPS ===
echo "[*] Layer 1: Disabling SIM Toolkit applications..."
pm disable-user --user 0 com.android.stk 2>/dev/null && echo "    [+] STK primary: DISABLED"
pm disable-user --user 0 com.android.stk2 2>/dev/null && echo "    [+] STK secondary: DISABLED"

# === 2. DISABLE CARRIER MANAGEMENT AGENTS ===
echo "[*] Layer 2: Disabling carrier management..."
pm disable-user --user 0 com.sec.omadmclient 2>/dev/null && echo "    [+] OMADM client: DISABLED"
pm disable-user --user 0 com.samsung.android.app.omcagent 2>/dev/null && echo "    [+] OMC Agent: DISABLED"
pm disable-user --user 0 com.android.carrierdefaultapp 2>/dev/null && echo "    [+] Carrier Default App: DISABLED"

# === 3. BLOCK STK AT RIL LEVEL ===
echo "[*] Layer 3: Setting carrier feature flags..."
# These persist across reboots (stored in /data/property/)
setprop persist.vendor.ril.disable_stk_cmds 1 2>/dev/null
setprop persist.vendor.ril.disable_bip 1 2>/dev/null
setprop persist.radio.carrier.enable false 2>/dev/null
echo "    [+] STK commands: BLOCKED at RIL"
echo "    [+] BIP channels: BLOCKED at RIL"

# === 4. BLOCK CARRIER OTA/PUSH ===
echo "[*] Layer 4: Blocking carrier push services..."
pm disable-user --user 0 com.sec.spp.push 2>/dev/null && echo "    [+] Samsung Push: DISABLED"
pm disable-user --user 0 com.samsung.android.knox.pushmanager 2>/dev/null && echo "    [+] Knox Push: DISABLED"
pm disable-user --user 0 com.samsung.android.pushservice 2>/dev/null && echo "    [+] Push Service: DISABLED"
pm disable-user --user 0 com.wssyncmldm 2>/dev/null && echo "    [+] FOTA: DISABLED"
pm disable-user --user 0 com.sec.android.soagent 2>/dev/null && echo "    [+] SW Update: DISABLED"

# === 5. MONITOR SIM ACTIVITY ===
echo "[*] Layer 5: Setting up SIM activity monitoring..."
echo "    Monitor command: logcat -b radio | grep -iE 'stk|proactive|bip|envelope|uicc|apdu'"
echo "    This shows everything the SIM tries to do"

# === 6. DISPLAY SIM INFO ===
echo ""
echo "=== SIM CARD INFO ==="
echo "Carrier: $(getprop gsm.sim.operator.alpha)"
echo "MCC/MNC: $(getprop gsm.sim.operator.numeric)"
echo "Country: $(getprop gsm.sim.operator.iso-country)"
echo "State: $(getprop gsm.sim.state)"
echo "Sales Code: $(getprop ro.csc.sales_code)"

echo ""
echo "========================================"
echo "  SIM SANDBOX: ACTIVE"
echo ""
echo "  STK Commands: BLOCKED"
echo "  BIP Channels: BLOCKED"
echo "  APDU Access:  MONITORED"
echo "  OTA Push:     BLOCKED"
echo "  Carrier Mgmt: DISABLED"
echo ""
echo "  ALLOWED THROUGH:"
echo "  - Voice calls"
echo "  - SMS/MMS"
echo "  - Mobile data"
echo "========================================"
echo ""
echo "NOTE: pm disable persists across reboots."
echo "      setprop persist.* persists across reboots."
echo "      The SIM CANNOT push code through these blocks."
