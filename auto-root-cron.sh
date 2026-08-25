#!/bin/bash
# JackKnife Auto-Root — Runs on iMac/Fodenn via cron
# Connects to S25 Ultra via Tailscale, roots, locks down
#
# Setup:
# 1. Enable "Wireless debugging" on the phone (Developer Options)
# 2. Pair once: adb pair <phone-ip>:port
# 3. Add to crontab: */5 * * * * /path/to/auto-root-cron.sh
#
# The script checks every 5 minutes if the phone needs rooting.

PHONE_IP="100.86.126.120"  # Tailscale IP
ADB_PORT="5555"
EXPLOIT="/data/local/tmp/cve-2026-43499"
ROOT_HELPER="/data/local/tmp/cve-2026-43499-root"
POST_ROOT="/data/local/tmp/jackknife-post-root.sh"
LOG="/tmp/jackknife-autoroot.log"

log() { echo "$(date '+%Y-%m-%d %H:%M:%S') $1" >> "$LOG"; }

# Try to connect
adb connect "${PHONE_IP}:${ADB_PORT}" > /dev/null 2>&1
sleep 2

# Check if phone is reachable
if ! adb devices 2>/dev/null | grep -q "${PHONE_IP}"; then
    exit 0  # Phone not on network, skip
fi

# Check if already rooted
ROOT_CHECK=$($ROOT_HELPER -c 'echo OK' 2>&1)
if echo "$ROOT_CHECK" | grep -q "OK"; then
    exit 0  # Already rooted, nothing to do
fi

# Phone is reachable but NOT rooted — ROOT IT!
log "Phone detected, not rooted. Firing GhostLock..."

adb -s "${PHONE_IP}:${ADB_PORT}" shell \
    "LD_PRELOAD=${EXPLOIT} /system/bin/true" > /dev/null 2>&1

sleep 5

# Verify root
ROOT_VERIFY=$(adb -s "${PHONE_IP}:${ADB_PORT}" shell \
    "${ROOT_HELPER} -c id" 2>&1)

if echo "$ROOT_VERIFY" | grep -q "uid=0"; then
    log "ROOT ACHIEVED: $ROOT_VERIFY"

    # Run post-root lockdown
    adb -s "${PHONE_IP}:${ADB_PORT}" shell \
        "${ROOT_HELPER} -c 'sh ${POST_ROOT}'" > /dev/null 2>&1
    log "Post-root lockdown complete"

    # Re-disable packages (in case of factory reset)
    for pkg in com.wssyncmldm com.sec.android.soagent com.android.stk com.android.stk2 \
               com.samsung.android.app.updatecenter com.sec.spp.push \
               com.samsung.android.knox.pushmanager com.google.mainline.telemetry \
               com.samsung.android.knox.analytics.uploader; do
        adb -s "${PHONE_IP}:${ADB_PORT}" shell \
            "pm disable-user --user 0 $pkg" > /dev/null 2>&1
    done
    log "Packages disabled"
else
    log "ROOT FAILED: $ROOT_VERIFY"
fi
