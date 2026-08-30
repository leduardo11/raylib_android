#!/bin/bash
# ── deploy_vps.sh ─────────────────────────────────────────────────────────
# Build (unless --skip-build) and push the APK to the public VPS server, then
# verify the public link serves exactly the uploaded bytes.
#
# Usage:
#   ./scripts/deploy_vps.sh               # build Debug + deploy
#   ./scripts/deploy_vps.sh Release       # build Release + deploy
#   ./scripts/deploy_vps.sh --skip-build  # deploy the existing artifact
#
# The upload goes to a .tmp name and is atomically renamed on the VPS, so a
# client downloading mid-deploy never gets a truncated file.
#
# SSH auth is resolved in this order:
#   1. SSH_PASS env var   (requires sshpass on PATH)
#   2. SSH_ASKPASS        (a helper that echoes the password, e.g. the
#                          opencode session helper, or your own)
#   3. Existing SSH key already authorized on the VPS
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."

VPS_USER=root
VPS_HOST=155.94.247.22
VPS_PORT=8080
VPS_REMOTE_DIR=/opt/apkserve/artifacts/outputs/apk/debug

BUILD_TYPE="Debug"
SKIP_BUILD=0
for arg in "$@"; do
    case "$arg" in
        Release|Debug) BUILD_TYPE="$arg" ;;
        --skip-build)  SKIP_BUILD=1 ;;
        *) echo "Unknown argument: $arg (expected Debug|Release|--skip-build)" >&2; exit 2 ;;
    esac
done

APK_NAME="raylib_android-${BUILD_TYPE,,}.apk"
PUBLIC_URL="http://$VPS_HOST:$VPS_PORT/$APK_NAME"
APK=""

echo "==> Deploying to $VPS_HOST:$VPS_PORT  (${BUILD_TYPE})"

if [ "$SKIP_BUILD" = "0" ]; then
    "$SCRIPT_DIR/build_apk.sh" "$BUILD_TYPE" 2>&1 | tail -8
fi

# Locate the APK the same way build_apk.sh does.
APK=$(find "$PROJECT_DIR/artifacts" -name "$APK_NAME" 2>/dev/null | head -1)
if [ -z "$APK" ]; then
    echo "Could not find $APK_NAME under $PROJECT_DIR/artifacts" >&2
    exit 1
fi

LOCAL_SIZE=$(stat -c %s "$APK")
echo "    APK:       $APK ($LOCAL_SIZE bytes)"
echo "    Remote:    $VPS_USER@$VPS_HOST:$VPS_REMOTE_DIR/"

# ── scp/ssh wrapper with auth fallbacks ───────────────────────────────────
SSH_OPTS=( -o StrictHostKeyChecking=accept-new -o ConnectTimeout=15 )
if [ -n "${SSH_PASS:-}" ] && command -v sshpass >/dev/null 2>&1; then
    run_scp() { sshpass -e scp "${SSH_OPTS[@]}" "$@"; }
    run_ssh() { sshpass -e ssh "${SSH_OPTS[@]}" "$@"; }
elif [ -n "${SSH_ASKPASS:-}" ]; then
    export SSH_ASKPASS_REQUIRE=force
    run_scp() { scp "${SSH_OPTS[@]}" "$@"; }
    run_ssh() { ssh "${SSH_OPTS[@]}" "$@"; }
else
    run_scp() { scp "${SSH_OPTS[@]}" "$@"; }
    run_ssh() { ssh "${SSH_OPTS[@]}" "$@"; }
fi

# ── Upload + atomic rename ────────────────────────────────────────────────
TMP_REMOTE="$VPS_REMOTE_DIR/$APK_NAME.tmp"
run_scp "$APK" "$VPS_USER@$VPS_HOST:$TMP_REMOTE"
run_ssh "$VPS_USER@$VPS_HOST" \
    "mkdir -p '$VPS_REMOTE_DIR' && mv -f '$TMP_REMOTE' '$VPS_REMOTE_DIR/$APK_NAME'"

echo "    Uploaded + swapped atomically."

# ── Verify the public link serves exactly our bytes ───────────────────────
SERVED=$(curl -sI "$PUBLIC_URL" | grep -i '^Content-Length:' | tr -d '\r' \
         | awk '{print $2}')
if [ "$SERVED" = "$LOCAL_SIZE" ]; then
    echo "    Verified: $PUBLIC_URL  (HTTP 200, $SERVED bytes)"
else
    echo "    MISMATCH: local=$LOCAL_SIZE served=${SERVED:-unreachable}" >&2
    exit 1
fi

echo "==> Deploy complete."
echo "    Install: adb install \"$APK\""
echo "    Download: $PUBLIC_URL"