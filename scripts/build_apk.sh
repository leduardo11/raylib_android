#!/bin/bash
# ── build_apk.sh ───────────────────────────────────────────────────────────
# Build the Android APK via Gradle, auto-sign, and prune stale artifacts.
# Usage: ./scripts/build_apk.sh [Debug|Release]
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/find_tools.sh"

PROJECT_DIR="$SCRIPT_DIR/.."
ANDROID_DIR="$PROJECT_DIR/src/app"
ARTIFACTS_DIR="$PROJECT_DIR/artifacts"

BUILD_TYPE="${1:-Debug}"

# Ensure all required tools exist
find_tools_java
find_tools_android_sdk
find_tools_ndk
find_tools_cmake

export JAVA_HOME
export ANDROID_HOME

# Find apksigner
APKSIGNER=""
for d in "$ANDROID_HOME/build-tools"/*/apksigner; do
    if [ -x "$d" ]; then APKSIGNER="$d"; break; fi
done

echo "==> Building raylib_android [$BUILD_TYPE]"
echo "    JAVA_HOME:    $JAVA_HOME"
echo "    ANDROID_HOME: $ANDROID_HOME"
echo "    apksigner:    ${APKSIGNER:-NOT FOUND}"

# Create local.properties if missing
cd "$ANDROID_DIR"
if [ ! -f local.properties ]; then
    echo "sdk.dir=${ANDROID_HOME}" > local.properties
    echo "    [created local.properties]"
fi

# Do NOT prune CMakeCache.txt here. Deleting it forces a full reconfigure in a
# populated build dir, and CMake 3.22 (Android SDK) rewrites CMakeCache.txt and
# the compiler-detect files with an mtime NEWER than the regenerated
# build.ninja. Those are implicit deps of the `build build.ninja: RERUN_CMAKE`
# edge, so ninja loops forever: "manifest 'build.ninja' still dirty after 100
# tries". AGP tracks configuration itself (fingerprint binaries + ninja regen),
# so no manual cache invalidation is needed. If the cache ever does go stale,
# remove artifacts/.cxx once and rebuild.

# Build
./gradlew assemble"${BUILD_TYPE}" --warning-mode all 2>&1

# Locate the APK: prefer the fresh Gradle output (default buildDir since
# commit 199b6a1 removed the artifacts/ override), then scan artifacts/ for
# any stale copies (old buildDir scheme) only as a last resort.
APK=""
for candidate in \
    "$ANDROID_DIR/build/outputs/apk/${BUILD_TYPE,,}/raylib_android-${BUILD_TYPE,,}.apk" \
    "$ANDROID_DIR/build/outputs/apk/${BUILD_TYPE,,}/app-${BUILD_TYPE,,}.apk" \
    $(find "$ARTIFACTS_DIR" -name "raylib_android-${BUILD_TYPE,,}.apk" 2>/dev/null); do
    if [ -n "$candidate" ] && [ -f "$candidate" ]; then
        APK="$candidate"
        break
    fi
done

if [ -n "$APK" ] && [ -f "$APK" ]; then
    # Sign the APK (debug builds are often unsigned on native-only APKs)
    if [ -n "$APKSIGNER" ]; then
        KS="$HOME/.android/debug.keystore"
        if [ ! -f "$KS" ]; then
            echo "    [creating debug keystore]"
            keytool -genkey -v -keystore "$KS" -storepass android \
                -alias androiddebugkey -keypass android -keyalg RSA \
                -validity 10000 -dname "CN=Android Debug,O=Android,C=US" 2>/dev/null
        fi
        echo "    [signing APK]"
        "$APKSIGNER" sign --ks "$KS" --ks-pass pass:android \
            --ks-key-alias androiddebugkey --key-pass pass:android \
            --v1-signing-enabled true --v2-signing-enabled true "$APK" 2>&1 || \
        echo "    [WARN] signing failed (APK may already be signed)"

        echo "    [verifying signature]"
        "$APKSIGNER" verify "$APK" 2>&1 && \
            echo "    [OK] APK signature verified (v1/v2)" || \
            echo "    [WARN] APK signature could not be verified"
    else
        echo "    [WARN] apksigner not found — APK is UNSIGNED"
    fi

    echo ""
    echo "==> APK ready: $(ls -lh "$APK" | awk '{print $5}')"
    echo "    $APK"
    echo "==> Install via ADB:"
    echo "    adb install \"$APK\""

    if [ -n "$ADB" ] && [ -x "$ADB" ]; then
        echo "    or:  ./scripts/adb_debug.sh install"
    fi
else
    echo "[ERROR] APK not found"
    exit 1
fi
