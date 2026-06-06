#!/bin/bash
# ── serve_apk.sh ──────────────────────────────────────────────────────────
# Serve APK + crash upload via Python HTTP server.
# Usage: ./scripts/serve_apk.sh [port]
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/find_tools.sh"

PORT="${1:-8080}"

if command -v python3 &>/dev/null; then
    exec python3 "$SCRIPT_DIR/serve_apk.py" --port "$PORT"
fi

echo "Python3 not found. Install Python 3 and try again." >&2
exit 1
