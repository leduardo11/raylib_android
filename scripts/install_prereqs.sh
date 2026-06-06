#!/bin/bash
# ── install_prereqs.sh ────────────────────────────────────────────────────
# Check and install prerequisites for building a raylib Android app.
# Detects what's installed and offers to install what's missing.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."
source "$SCRIPT_DIR/find_tools.sh"

echo ""
echo "=========================================="
echo " raylib Android — Prerequisites Check"
echo "=========================================="

find_tools_all

echo ""
echo "--- Additional checks ---"

# ── Gradle wrapper ────────────────────────────────────────────────────────
GRADLE_DIR="$PROJECT_DIR/src/app"
if [ -f "$GRADLE_DIR/gradlew" ]; then
    echo "  gradlew: OK"
else
    echo "  [INFO] Will auto-generate on first build"
fi

echo ""
echo "=========================================="
echo " To build: ./scripts/build_apk.sh"
echo " To install: ./scripts/adb_debug.sh install"
echo "=========================================="
