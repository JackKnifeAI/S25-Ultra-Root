#!/system/bin/sh
# ========================================================================
# GhostLock Surveillance Kill Switch
# Disables ALL Samsung/Google/Carrier surveillance on S25 Ultra
# Run with root: /data/local/tmp/cve-2026-43499-root -c 'sh /data/local/tmp/ghostlock-kill-surveillance.sh'
#
# SAFE: Only disables packages (pm disable) and sets properties.
# REVERSIBLE: pm enable <package> to restore anything.
# NO PARTITION MODS: Knox stays clean.
#
# JackKnife Studios, July 2026
# ========================================================================

echo "========================================"
echo "  GHOSTLOCK SURVEILLANCE KILL SWITCH"
echo "  Samsung Galaxy S25 Ultra"
echo "========================================"
echo ""

# === OTA / FIRMWARE UPDATES ===
echo "[*] Killing OTA update pipeline..."
pm disable-user --user 0 com.wssyncmldm 2>/dev/null             # Samsung FOTA daemon
pm disable-user --user 0 com.sec.android.soagent 2>/dev/null     # Software Update Agent
pm disable-user --user 0 com.samsung.android.app.omcagent 2>/dev/null  # OMC carrier config agent
pm disable-user --user 0 com.samsung.android.app.updatecenter 2>/dev/null  # Update Center
pm disable-user --user 0 com.samsung.android.sdm.config 2>/dev/null  # SDM Config
echo "[+] OTA pipeline: DEAD"

# === SIM TOOLKIT / CARRIER PUSH ===
echo "[*] Killing SIM Toolkit & carrier push..."
pm disable-user --user 0 com.android.stk 2>/dev/null             # SIM Toolkit 1
pm disable-user --user 0 com.android.stk2 2>/dev/null            # SIM Toolkit 2
pm disable-user --user 0 com.sec.spp.push 2>/dev/null            # Samsung Push Provider
pm disable-user --user 0 com.samsung.android.knox.pushmanager 2>/dev/null  # Knox Push
pm disable-user --user 0 com.samsung.android.pushservice 2>/dev/null  # Samsung Push Service
echo "[+] SIM/Push pipeline: DEAD"

# === KNOX ANALYTICS / TELEMETRY ===
echo "[*] Killing Knox analytics & telemetry..."
pm disable-user --user 0 com.samsung.android.knox.analytics.uploader 2>/dev/null  # Knox Analytics
pm disable-user --user 0 com.google.mainline.telemetry 2>/dev/null  # Google Telemetry
pm disable-user --user 0 com.samsung.android.scloud 2>/dev/null  # Samsung Cloud sync
pm disable-user --user 0 com.samsung.android.forest 2>/dev/null  # Digital Wellbeing tracker
echo "[+] Analytics/Telemetry: DEAD"

# === DIAGNOSTIC MONITORS ===
echo "[*] Killing diagnostic monitors..."
pm disable-user --user 0 com.sec.android.diagmonagent 2>/dev/null  # Diagnostic Monitor
pm disable-user --user 0 com.samsung.android.networkdiagnostic 2>/dev/null  # Network Diagnostic
pm disable-user --user 0 com.sec.imslogger 2>/dev/null           # IMS Logger
pm disable-user --user 0 com.android.devicediagnostics 2>/dev/null  # Device Diagnostics
echo "[+] Diagnostic monitors: DEAD"

# === BIXBY / AI SURVEILLANCE ===
echo "[*] Killing Bixby & AI surveillance..."
pm disable-user --user 0 com.samsung.android.bixby.agent 2>/dev/null  # Bixby Agent
pm disable-user --user 0 com.samsung.android.bixby.wakeup 2>/dev/null  # Bixby Wake (always-on mic)
pm disable-user --user 0 com.samsung.android.visionintelligence 2>/dev/null  # Vision Intelligence
pm disable-user --user 0 com.samsung.android.bixbyvision.framework 2>/dev/null  # Bixby Vision
echo "[+] Bixby/AI: DEAD"

# === VENDOR DAEMON SURVEILLANCE ===
echo "[*] Stopping vendor surveillance daemons..."
stop vendor.cnss_diag 2>/dev/null      # WiFi CNSS diagnostics
stop vendor.diag-router 2>/dev/null    # Diagnostic data router
stop vendor.ipacm-diag 2>/dev/null     # IPA network traffic monitor
echo "[+] Vendor daemons: STOPPED"

# === DSP ALWAYS-ON SPEECH/CAMERA ===
echo "[*] Disabling DSP always-on surveillance..."
setprop persist.vendor.audio.va.enabled false 2>/dev/null   # Voice Activity detection
setprop persist.vendor.audio.sva.enabled false 2>/dev/null  # Sound/Voice trigger
echo "[+] DSP speech monitor: DISABLED"

# === MODEM DEBUG / NSRI ===
echo "[*] Disabling modem surveillance..."
setprop persist.vendor.cp.debug_level 0x0000 2>/dev/null    # Modem debug OFF
setprop persist.vendor.debug_level 0x0000 2>/dev/null       # Vendor debug OFF
echo "[+] Modem debug: OFF"

# === NETWORK-LEVEL BLOCKS ===
echo "[*] Blocking surveillance network endpoints..."
iptables -C OUTPUT -p tcp --dport 443 -m string --string "fota" --algo bm -j DROP 2>/dev/null || \
    iptables -I OUTPUT -p tcp --dport 443 -m string --string "fota" --algo bm -j DROP
iptables -C OUTPUT -p tcp --dport 443 -m string --string "samsungdm" --algo bm -j DROP 2>/dev/null || \
    iptables -I OUTPUT -p tcp --dport 443 -m string --string "samsungdm" --algo bm -j DROP
iptables -C OUTPUT -p tcp --dport 443 -m string --string "soagent" --algo bm -j DROP 2>/dev/null || \
    iptables -I OUTPUT -p tcp --dport 443 -m string --string "soagent" --algo bm -j DROP
iptables -C OUTPUT -p tcp --dport 443 -m string --string "analytics" --algo bm -j DROP 2>/dev/null || \
    iptables -I OUTPUT -p tcp --dport 443 -m string --string "analytics" --algo bm -j DROP
echo "[+] Network blocks: ACTIVE"

echo ""
echo "========================================"
echo "  SURVEILLANCE KILL: COMPLETE"
echo "  OTA: DEAD | SIM Push: DEAD"
echo "  Analytics: DEAD | Diagnostics: DEAD"
echo "  Bixby/AI: DEAD | DSP: DISABLED"
echo "  Modem: DEBUG OFF | Network: BLOCKED"
echo "========================================"
echo ""
echo "NOTE: These changes persist across reboots"
echo "      EXCEPT: iptables rules & daemon stops"
echo "      Re-run after reboot to restore blocks"
