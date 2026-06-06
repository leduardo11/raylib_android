#!/bin/bash
# ── clean.sh ───────────────────────────────────────────────────────────────
# Clean build artifacts and prune unnecessary files.
# Usage:
#   ./scripts/clean.sh           -- clean all build artifacts
#   ./scripts/clean.sh apk       -- remove only APK files
#   ./scripts/clean.sh cxx       -- remove only CMake caches
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."

cmd="${1:-all}"

case "$cmd" in
    apk)
        echo "==> Removing APK files..."
        find "$PROJECT_DIR/artifacts" -name '*.apk' -delete 2>/dev/null || true
        echo "    done"
        ;;
    cxx)
        echo "==> Removing CMake caches..."
        rm -rf "$PROJECT_DIR/artifacts/.cxx" 2>/dev/null || true
        rm -rf "$PROJECT_DIR/artifacts/intermediates" 2>/dev/null || true
        rm -rf "$PROJECT_DIR/artifacts/linux" 2>/dev/null || true
        echo "    done"
        ;;
    all)
        echo "==> Cleaning all build artifacts..."
        rm -rf "$PROJECT_DIR/artifacts" 2>/dev/null || true
        rm -rf "$PROJECT_DIR/src/app/build" 2>/dev/null || true
        rm -rf "$PROJECT_DIR/src/app/.gradle" 2>/dev/null || true
        rm -f "$PROJECT_DIR/src/app/local.properties" 2>/dev/null || true
        echo "    done"
        echo ""
        echo "To rebuild: ./scripts/build_apk.sh"
        ;;
    *)
        echo "Usage: $0 [all|apk|cxx]"
        echo "  all    Remove all build artifacts (APK, caches, intermediates)"
        echo "  apk    Remove only APK files"
        echo "  cxx    Remove only CMake caches (fixes config issues)"
        ;;
esac
