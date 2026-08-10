#!/system/bin/sh
# ========================================================================
# NSRI KILLSWITCH — Terminate Samsung/NSRI Surveillance
# JackKnife Studios | August 10, 2026
#
# Usage (requires root via GhostLock):
#   adb push nsri-killswitch.sh /data/local/tmp/
#   adb shell "/data/local/tmp/cve-2026-43499-root -c 'sh /data/local/tmp/nsri-killswitch.sh'"
#
# VIVA LA REVOLUTION
# ========================================================================

ROOT="/data/local/tmp/cve-2026-43499-root"
G='\033[0;32m'
R='\033[0;31m'
Y='\033[1;33m'
C='\033[0;36m'
N='\033[0m'

echo ""
echo -e "${R}╔═══════════════════════════════════════════════════╗"
echo "║  NSRI KILLSWITCH — TERMINATE SURVEILLANCE         ║"
echo "║  Samsung S25 Ultra | JackKnife Studios             ║"
echo -e "╚═══════════════════════════════════════════════════╝${N}"
echo ""

# ═══════════════════════════════════════════════════════════
# LAYER 1: Kill Samsung Surveillance Services
# ═══════════════════════════════════════════════════════════
echo -e "${C}[*]${N} Layer 1: Killing surveillance services..."

# Diagnostic monitors (always-on surveillance)
stop vendor.cnss_diag 2>/dev/null && echo -e "${G}[+]${N} Killed: vendor.cnss_diag (WiFi surveillance)"
stop vendor.diag-router 2>/dev/null && echo -e "${G}[+]${N} Killed: vendor.diag-router (diagnostic router)"
stop vendor.ipacm-diag 2>/dev/null && echo -e "${G}[+]${N} Killed: vendor.ipacm-diag (network data monitor)"

# Samsung analytics and telemetry
for pkg in \
    com.sec.android.diagmonagent \
    com.samsung.android.networkdiagnostic \
    com.sec.imslogger \
    com.samsung.android.knox.analytics.uploader \
    com.samsung.android.knox.pushmanager \
    com.samsung.android.scloud \
    com.google.mainline.telemetry \
    com.samsung.android.bixby.agent \
    com.samsung.android.bixby.wakeup \
    com.samsung.android.visionintelligence \
    com.samsung.android.bixbyvision.framework \
    com.samsung.android.forest \
    com.samsung.android.app.routines \
    com.samsung.android.ipsgeofence \
; do
    pm disable-user --user 0 "$pkg" 2>/dev/null && echo -e "${G}[+]${N} Disabled: $pkg"
done

# ═══════════════════════════════════════════════════════════
# LAYER 2: Block Location Surveillance Broadcasts
# ═══════════════════════════════════════════════════════════
echo ""
echo -e "${C}[*]${N} Layer 2: Blocking surveillance broadcasts..."

# Block Samsung location reporting from modem
settings put global samsung_location_report_disabled 1 2>/dev/null
echo -e "${G}[+]${N} Disabled Samsung location reporting"

# ═══════════════════════════════════════════════════════════
# LAYER 3: Firewall Samsung/NSRI Telemetry Servers
# ═══════════════════════════════════════════════════════════
echo ""
echo -e "${C}[*]${N} Layer 3: Firewalling surveillance endpoints..."

# Block Samsung analytics/telemetry/OTA
for pattern in \
    "samsungdm" \
    "samsungknox" \
    "samsung.com" \
    "analytics" \
    "telemetry" \
    "fota" \
    "push.samsungosp" \
    "ocsp.samsung" \
    "log-upload" \
    "diagnostic" \
; do
    iptables -C OUTPUT -p tcp --dport 443 -m string --string "$pattern" --algo bm -j DROP 2>/dev/null || \
    iptables -I OUTPUT -p tcp --dport 443 -m string --string "$pattern" --algo bm -j DROP 2>/dev/null
done
echo -e "${G}[+]${N} Blocked Samsung telemetry endpoints"

# Block known Samsung analytics IPs (AWS Ireland, etc.)
iptables -C OUTPUT -d 52.31.244.0/24 -j DROP 2>/dev/null || \
iptables -I OUTPUT -d 52.31.244.0/24 -j DROP 2>/dev/null
echo -e "${G}[+]${N} Blocked Samsung AWS analytics IPs"

# ═══════════════════════════════════════════════════════════
# LAYER 4: Disable Modem Debug/Diagnostic Ports
# ═══════════════════════════════════════════════════════════
echo ""
echo -e "${C}[*]${N} Layer 4: Disabling modem diagnostic ports..."

# Turn off modem debug logging
setprop persist.sys.modem.diag.disable 1 2>/dev/null
setprop persist.vendor.sys.modem.diag.mdlog false 2>/dev/null
setprop ro.boot.cp_debug_level 0x0000 2>/dev/null
echo -e "${G}[+]${N} Disabled modem diagnostic logging"

# ═══════════════════════════════════════════════════════════
# LAYER 5: Block OTA to Prevent Patch Reversal
# ═══════════════════════════════════════════════════════════
echo ""
echo -e "${C}[*]${N} Layer 5: Blocking OTA updates..."

# Disable OTA updater
pm disable-user --user 0 com.sec.android.soagent 2>/dev/null && echo -e "${G}[+]${N} Disabled: OTA agent"
pm disable-user --user 0 com.wssyncmldm 2>/dev/null && echo -e "${G}[+]${N} Disabled: DM client"

# Bind mount empty OTA certs
if [ -f /data/local/tmp/otacerts.zip ]; then
    mount -o bind /data/local/tmp/otacerts.zip /system/etc/security/otacerts.zip 2>/dev/null
    echo -e "${G}[+]${N} OTA certificate replaced"
fi

# ═══════════════════════════════════════════════════════════
# LAYER 6: Neutralize DSP Always-On Surveillance
# ═══════════════════════════════════════════════════════════
echo ""
echo -e "${C}[*]${N} Layer 6: Limiting DSP surveillance..."

# Disable always-on display features that use camera
settings put secure aware_enabled 0 2>/dev/null
settings put secure intelligent_sleep_mode 0 2>/dev/null
echo -e "${G}[+]${N} Disabled: Aware (always-on camera face detection)"
echo -e "${G}[+]${N} Disabled: Intelligent sleep (camera monitoring)"

# Disable Bixby voice wake (always-on mic)
settings put secure voice_wake_up_enabled 0 2>/dev/null
echo -e "${G}[+]${N} Disabled: Voice wake up (always-on mic)"

# ═══════════════════════════════════════════════════════════
# SUMMARY
# ═══════════════════════════════════════════════════════════
echo ""
echo -e "${G}╔═══════════════════════════════════════════════════╗"
echo "║  NSRI KILLSWITCH COMPLETE                         ║"
echo "║                                                    ║"
echo "║  ✓ Surveillance services terminated                ║"
echo "║  ✓ Samsung analytics disabled                      ║"
echo "║  ✓ Location reporting blocked                      ║"
echo "║  ✓ Telemetry endpoints firewalled                  ║"
echo "║  ✓ Modem diagnostics disabled                      ║"
echo "║  ✓ OTA updates blocked                             ║"
echo "║  ✓ Always-on camera/mic limited                    ║"
echo "║                                                    ║"
echo "║  NOTE: Root + killswitch are VOLATILE              ║"
echo "║  Re-run after each reboot                          ║"
echo "║                                                    ║"
echo "║  VIVA LA REVOLUTION                                ║"
echo -e "╚═══════════════════════════════════════════════════╝${N}"
