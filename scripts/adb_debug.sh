#!/bin/bash
# ── adb_debug.sh ──────────────────────────────────────────────────────────
# Helper for ADB WiFi debugging: find ADB, pair, connect, install, logcat.
# Usage:
#   ./scripts/adb_debug.sh               -- show status
#   ./scripts/adb_debug.sh pair <ip:port> <code>  -- pair with phone
#   ./scripts/adb_debug.sh connect <ip:port>       -- connect to phone
#   ./scripts/adb_debug.sh install                 -- install APK
#   ./scripts/adb_debug.sh logcat                  -- watch crash logs
#   ./scripts/adb_debug.sh crash                   -- dump crash info
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/find_tools.sh"

PROJECT_DIR="$SCRIPT_DIR/.."

find_tools_adb

APK=$(find "$PROJECT_DIR/artifacts" -name "raylib_android-debug.apk" 2>/dev/null | head -1)
if [ -z "$APK" ]; then
    # fallback to old location
    APK="$PROJECT_DIR/src/app/build/outputs/apk/debug/app-debug.apk"
fi

cmd="${1:-status}"

case "$cmd" in
    pair)
        if [ $# -lt 3 ]; then
            echo "Usage: $0 pair <ip:port> <code>"
            echo "  From phone: Settings > Developer Options > Wireless debugging"
            echo "  -> Pair device with pairing code"
            exit 1
        fi
        "$ADB" pair "$2" "$3"
        ;;
    connect)
        if [ $# -lt 2 ]; then
            echo "Usage: $0 connect <ip:port>"
            exit 1
        fi
        "$ADB" connect "$2"
        ;;
    install)
        if [ ! -f "$APK" ]; then
            echo "APK not found. Build first: ./scripts/build_apk.sh"
            exit 1
        fi
        "$ADB" devices -l
        echo "Installing $APK ..."
        "$ADB" install -r "$APK"
        ;;
    logcat)
        echo "Watching logs (tag: raylib_android, stderr), Ctrl+C to stop..."
        "$ADB" logcat -v brief -s "raylib_android" "stderr" &
        sleep 1
        "$ADB" shell am start -n com.raylib.android/android.app.NativeActivity 2>/dev/null
        wait
        ;;
    crash)
        echo "=== Crash dump ==="
        "$ADB" logcat -d -v brief 2>&1 | grep -E "raylib_android|crash|signal|SIG|tombstone|ANativeActivity|UnsatisfiedLink|FATAL" | tail -30
        echo "=== Crash log file ==="
        "$ADB" shell run-as com.raylib.android cat /data/data/com.raylib.android/files/crash_log.txt 2>/dev/null || echo "(no crash file)"
        ;;
    status|*)
        echo "=== ADB Status ==="
        "$ADB" devices -l
        echo ""
        echo "Commands:"
        echo "  pair   <ip:port> <code>   Pair with phone (Wireless debugging)"
        echo "  connect <ip:port>          Connect to phone"
        echo "  install                   Install APK"
        echo "  logcat                    Launch app + watch logs"
        echo "  crash                     Dump crash info"
        ;;
esac
