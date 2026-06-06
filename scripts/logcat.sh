#!/bin/bash
# ── logcat.sh ─────────────────────────────────────────────────────────────
# View Android logs filtered for the app.
# Usage:
#   ./scripts/logcat.sh              -- live logcat (raylib_android + stderr)
#   ./scripts/logcat.sh dump         -- dump recent logs
#   ./scripts/logcat.sh crash        -- show crash-only logs
#   ./scripts/logcat.sh install      -- install APK + live logcat
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/find_tools.sh"

find_tools_adb

cmd="${1:-live}"

case "$cmd" in
    live)
        echo "Live logcat (raylib_android + stderr). Ctrl+C to stop."
        echo "  Filters: raylib_android, stderr, OpenGL, raylib"
        echo "  Install: ./scripts/logcat.sh install"
        echo "---"
        "$ADB" logcat -v brief -s "raylib_android" "stderr" "OpenGL" "raylib"
        ;;
    dump)
        shift
        lines="${1:-50}"
        echo "=== Last $lines log lines ==="
        "$ADB" logcat -d -v brief 2>&1 | grep -E "raylib_android|stderr|OpenGL|raylib" | tail -"$lines"
        ;;
    crash)
        echo "=== Crash dump ==="
        "$ADB" logcat -d -v brief 2>&1 | grep -E "crash|signal|SIG|FATAL|tombstone|ANativeActivity|UnsatisfiedLink|raylib_android" | tail -40
        echo "---"
        "$ADB" shell run-as com.raylib.android cat /data/data/com.raylib.android/files/crash_log.txt 2>/dev/null || echo "(no crash file)"
        echo "---"
        echo "Full native crash info:"
        "$ADB" logcat -d -v brief 2>&1 | grep -i "DEBUG\|CRASH\|signal\|abort" | tail -20
        ;;
    install)
        APK=$(find "$SCRIPT_DIR/.." -name "raylib_android-debug.apk" 2>/dev/null | head -1)
        if [ -z "$APK" ]; then
            echo "APK not found. Build first: ./scripts/build_apk.sh"
            exit 1
        fi
        echo "Installing $APK ..."
        "$ADB" install -r "$APK" 2>&1 || {
            echo ""
            echo "Install failed. Common fixes:"
            echo "  1. Enable 'Install via USB' in Developer Options"
            echo "  2. If 'INSTALL_FAILED_UPDATE_INCOMPATIBLE': uninstall first:"
            echo "     adb uninstall com.raylib.android"
            echo "  3. Check device connection: ./scripts/adb_debug.sh"
            exit 1
        }
        echo "Launching app + watching logs..."
        sleep 1
        "$ADB" shell am start -n com.raylib.android/android.app.NativeActivity 2>/dev/null || true
        "$ADB" logcat -v brief -s "raylib_android" "stderr"
        ;;
    verify)
        APK=$(find "$SCRIPT_DIR/.." -name "raylib_android-debug.apk" 2>/dev/null | head -1)
        if [ -z "$APK" ] || [ ! -f "$APK" ]; then
            echo "APK not found. Build first: ./scripts/build_apk.sh"
            exit 1
        fi
        echo "=== APK Verification ==="
        echo "File: $APK"
        echo "Size: $(ls -lh "$APK" | awk '{print $5}')"
        echo ""
        echo "--- Signature ---"
        unzip -l "$APK" 2>/dev/null | grep 'META-INF' | awk '{print $4}'
        echo ""
        if unzip -p "$APK" META-INF/MANIFEST.MF 2>/dev/null | head -5; then
            echo "  (signed)"
        else
            echo "  ** UNSIGNED **"
        fi
        echo ""
        echo "--- Contents ---"
        unzip -l "$APK" 2>/dev/null | grep -E '\.so$' | awk '{print $4, $1}'
        echo ""
        echo "--- Install test ---"
        "$ADB" install -r "$APK" 2>&1 | head -3 || true
        ;;
    *)
        echo "Usage: $0 [live|dump|crash|install|verify]"
        echo ""
        echo "Commands:"
        echo "  live              Watch live logs (default)"
        echo "  dump [N]          Show last N log lines"
        echo "  crash             Dump crash info"
        echo "  install           Install APK + watch logs"
        echo "  verify            Show APK details"
        ;;
esac
