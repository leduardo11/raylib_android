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

# Extract version from build.gradle
VERSION_NAME=$(grep -E "versionName\s+" "$PROJECT_DIR/src/app/build.gradle" | head -1 | sed -E "s/.*versionName\s+['\"]([^'\"]+)['\"].*/\1/")
VERSION_CODE=$(grep -E "versionCode\s+" "$PROJECT_DIR/src/app/build.gradle" | head -1 | sed -E "s/.*versionCode\s+([0-9]+).*/\1/")
GIT_COMMIT=$(git -C "$PROJECT_DIR" rev-parse --short HEAD 2>/dev/null || echo "unknown")

APK_BASE_NAME="raylib_android-${BUILD_TYPE,,}"
APK_VERSIONED_NAME="${APK_BASE_NAME}-v${VERSION_NAME}-${GIT_COMMIT}.apk"
APK_NAME="${APK_VERSIONED_NAME}"
PUBLIC_URL="http://$VPS_HOST:$VPS_PORT/$APK_NAME"
APK=""

# Also deploy a version.json for the server to consume
VERSION_JSON_NAME="${APK_BASE_NAME}-version.json"

echo "==> Deploying to $VPS_HOST:$VPS_PORT  (${BUILD_TYPE})"

if [ "$SKIP_BUILD" = "0" ]; then
    "$SCRIPT_DIR/build_apk.sh" "$BUILD_TYPE" 2>&1 | tail -8
fi

# Locate the APK the same way build_apk.sh does: prefer the fresh Gradle
# output (default buildDir since commit 199b6a1), then stale artifacts/
# copies from the old buildDir scheme only as a last resort.
BUILT_APK=""
for candidate in \
    "$PROJECT_DIR/src/app/build/outputs/apk/${BUILD_TYPE,,}/raylib_android-${BUILD_TYPE,,}.apk" \
    "$PROJECT_DIR/src/app/build/outputs/apk/${BUILD_TYPE,,}/app-${BUILD_TYPE,,}.apk" \
    $(find "$PROJECT_DIR/artifacts" -name "raylib_android-${BUILD_TYPE,,}.apk" 2>/dev/null); do
    if [ -n "$candidate" ] && [ -f "$candidate" ]; then
        BUILT_APK="$candidate"
        break
    fi
done
if [ -z "$BUILT_APK" ]; then
    echo "Could not find built APK under $PROJECT_DIR/src/app/build/outputs or $PROJECT_DIR/artifacts" >&2
    exit 1
fi

# Copy to versioned name for upload
APK="$PROJECT_DIR/artifacts/outputs/apk/${BUILD_TYPE,,}/$APK_VERSIONED_NAME"
mkdir -p "$(dirname "$APK")"
cp "$BUILT_APK" "$APK"

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

# ── Create version.json ───────────────────────────────────────────────────
VERSION_JSON=$(mktemp)
cat > "$VERSION_JSON" <<EOF
{
  "versionName": "$VERSION_NAME",
  "versionCode": $VERSION_CODE,
  "gitCommit": "$GIT_COMMIT",
  "buildType": "$BUILD_TYPE",
  "apkName": "$APK_VERSIONED_NAME",
  "buildDate": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
  "sizeBytes": $LOCAL_SIZE
}
EOF

# ── Upload + atomic rename ────────────────────────────────────────────────
TMP_REMOTE="$VPS_REMOTE_DIR/$APK_NAME.tmp"
run_scp "$APK" "$VPS_USER@$VPS_HOST:$TMP_REMOTE"
run_ssh "$VPS_USER@$VPS_HOST" \
    "mkdir -p '$VPS_REMOTE_DIR' && mv -f '$TMP_REMOTE' '$VPS_REMOTE_DIR/$APK_NAME'"

# Upload version.json
TMP_JSON="$VPS_REMOTE_DIR/$VERSION_JSON_NAME.tmp"
run_scp "$VERSION_JSON" "$VPS_USER@$VPS_HOST:$TMP_JSON"
run_ssh "$VPS_USER@$VPS_HOST" \
    "mv -f '$TMP_JSON' '$VPS_REMOTE_DIR/$VERSION_JSON_NAME'"

# Create/update stable "latest" symlink on VPS
run_ssh "$VPS_USER@$VPS_HOST" \
    "cd '$VPS_REMOTE_DIR' && ln -sf '$APK_VERSIONED_NAME' '${APK_BASE_NAME}-latest.apk' && ln -sf '$VERSION_JSON_NAME' '${APK_BASE_NAME}-latest-version.json'"

# Upload the updated server script + restart it. The server is started as a
# fully-detached process. Kill and start must be SEPARATE SSH calls: a single
# combined command makes `pkill -f 'serve_apk.py --port'` match the restart's
# own shell command line (which contains that literal string) and kill itself
# before the new server can start.
run_scp "$SCRIPT_DIR/serve_apk.py" "$VPS_USER@$VPS_HOST:/opt/apkserve/scripts/serve_apk.py"
run_ssh "$VPS_USER@$VPS_HOST" "pkill -9 -f '[s]erve_apk.py --port' 2>/dev/null; sleep 1"
run_ssh "$VPS_USER@$VPS_HOST" \
    "cd /opt/apkserve && (setsid python3 scripts/serve_apk.py --port $VPS_PORT </dev/null >server.log 2>&1 &) ; sleep 2"

rm -f "$VERSION_JSON"

echo "    Uploaded + swapped atomically."
echo "    Version: $VERSION_NAME (code $VERSION_CODE, commit $GIT_COMMIT)"

# ── Verify the public link serves exactly our bytes ───────────────────────
SERVED=$(curl -sI "$PUBLIC_URL" | grep -i '^Content-Length:' | tr -d '\r' \
         | awk '{print $2}')
if [ "$SERVED" = "$LOCAL_SIZE" ]; then
    echo "    Verified: $PUBLIC_URL  (HTTP 200, $SERVED bytes)"
else
    echo "    MISMATCH: local=$LOCAL_SIZE served=${SERVED:-unreachable}" >&2
    exit 1
fi

# Verify version.json
VERSION_JSON_URL="http://$VPS_HOST:$VPS_PORT/$VERSION_JSON_NAME"
SERVED_JSON=$(curl -sI "$VERSION_JSON_URL" | grep -i '^Content-Length:' | tr -d '\r' \
              | awk '{print $2}')
if [ -n "$SERVED_JSON" ]; then
    echo "    Version JSON: $VERSION_JSON_URL"
fi

echo "==> Deploy complete."
echo "    Install: adb install \"$APK\""
echo "    Download: $PUBLIC_URL"
echo "    Latest (stable): http://$VPS_HOST:$VPS_PORT/${APK_BASE_NAME}-latest.apk"
echo "    Version info:    http://$VPS_HOST:$VPS_PORT/${APK_BASE_NAME}-latest-version.json"