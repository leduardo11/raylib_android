# ── find_tools.sh ─────────────────────────────────────────────────────────
# Common dependency detection and guided install for all raylib_android scripts.
# Source this file:  source "$(dirname "$0")/find_tools.sh"
#
# Exports:
#   ADB          — path to adb binary (will be installed if missing)
#   JAVA_HOME    — JDK path (will be installed if missing)
#   ANDROID_HOME — Android SDK root (will be installed if missing)
#   ANDROID_SDK  — alias for ANDROID_HOME
#   CMAKE        — path to cmake (prompted if missing)
#   NDK_VERSION  — NDK version from build.gradle (installed if missing)
#
# Usage:
#   source "$SCRIPT_DIR/find_tools.sh"
#   find_tools_adb           -> sets $ADB, installs if missing
#   find_tools_java          -> sets $JAVA_HOME, installs if missing
#   find_tools_android_sdk   -> sets $ANDROID_HOME, installs if missing
#   find_tools_ndk           -> reads NDK version from build.gradle, installs if missing
#   find_tools_cmake         -> sets $CMAKE, prompts if missing
#   find_tools_all           -> runs all of the above

prompt_install() {
    local name="$1"
    local install_cmd="$2"
    local post_msg="$3"
    echo ""
    echo "[!] $name not found."
    echo "    Install command: $install_cmd"
    echo ""
    printf "    Install now? [Y/n] "
    read -r reply
    case "$reply" in
        n|N|no|NO) echo "    Skipping. $post_msg"; return 1 ;;
        *) echo "    Installing..."; eval "$install_cmd"; return 0 ;;
    esac
}

# ── ADB ───────────────────────────────────────────────────────────────────
find_tools_adb() {
    local adb_candidates
    ADB=""
    # Known locations
    for p in "$HOME/Library/Android/sdk/platform-tools/adb" \
             "$HOME/Android/Sdk/platform-tools/adb" \
             /usr/local/share/android-sdk/platform-tools/adb; do
        if [ -x "$p" ]; then ADB="$p"; break; fi
    done
    # PATH fallback
    if [ -z "$ADB" ]; then
        ADB="$(command -v adb 2>/dev/null || true)"
    fi
    if [ -z "$ADB" ]; then
        echo ""
        echo "--- ADB not found ---"
        if [[ "$(uname)" == "Darwin" ]]; then
            prompt_install "ADB" \
                'brew install --cask android-platform-tools 2>/dev/null || echo "  Fallback: download from https://developer.android.com/studio/releases/platform-tools"' \
                "Set ANDROID_HOME or add platform-tools to PATH manually."
            ADB="$(command -v adb 2>/dev/null || echo "$HOME/Library/Android/sdk/platform-tools/adb")"
        else
            prompt_install "ADB" \
                'sudo apt-get install -y android-sdk-platform-tools 2>/dev/null || echo "  Install Android SDK platform-tools manually."' \
                "Install platform-tools and re-run."
            ADB="$(command -v adb 2>/dev/null || true)"
        fi
        if [ -z "$ADB" ] || [ ! -x "$ADB" ]; then
            # Final fallback — try inside Android SDK
            for p in "$HOME/Library/Android/sdk/platform-tools/adb" "$HOME/Android/Sdk/platform-tools/adb"; do
                if [ -x "$p" ]; then ADB="$p"; break; fi
            done
        fi
    fi
    if [ -n "$ADB" ] && [ -x "$ADB" ]; then
        echo "  adb: $ADB"
    else
        echo "  [WARN] adb not found — some features unavailable."
    fi
    export ADB
}

# ── Java / JDK ────────────────────────────────────────────────────────────
find_tools_java() {
    if [ -z "$JAVA_HOME" ]; then
        for dir in "$HOME/Library/Java/jdk-17"*"/Contents/Home" \
                   "$HOME/.sdkman/candidates/java/current" \
                   /usr/lib/jvm/java-17-openjdk-* \
                   /Applications/Android\ Studio.app/Contents/jbr/Contents/Home \
                   /Applications/Android\ Studio.app/Contents/jbr; do
            if [ -d "$dir" ]; then
                export JAVA_HOME="$dir"
                break
            fi
        done
    fi
    if [ -z "$JAVA_HOME" ]; then
        local java_bin
        java_bin="$(command -v java 2>/dev/null || true)"
        if [ -n "$java_bin" ]; then
            if [[ "$(uname)" == "Darwin" ]]; then
                JAVA_HOME="$(/usr/libexec/java_home 2>/dev/null || dirname "$(dirname "$java_bin")")"
            else
                JAVA_HOME="$(dirname "$(dirname "$(readlink -f "$java_bin")")")"
            fi
            export JAVA_HOME
        fi
    fi
    if [ -z "$JAVA_HOME" ]; then
        echo ""
        echo "--- Java JDK not found ---"
        if [[ "$(uname)" == "Darwin" ]]; then
            prompt_install "JDK 17+" \
                'brew install openjdk@17' \
                "Set JAVA_HOME manually or install JDK from https://adoptium.net/"
            for d in /opt/homebrew/opt/openjdk@17 /usr/local/opt/openjdk@17; do
                if [ -d "$d" ]; then export JAVA_HOME="$d"; break; fi
            done
            if [ -z "$JAVA_HOME" ] && [ -d "/Library/Java/JavaVirtualMachines" ]; then
                JAVA_HOME="$(ls -d /Library/Java/JavaVirtualMachines/openjdk*.jdk/Contents/Home 2>/dev/null | head -1)"
            fi
        else
            prompt_install "JDK 17+" \
                'sudo apt-get install -y openjdk-17-jdk' \
                "Set JAVA_HOME manually."
            for d in /usr/lib/jvm/java-17-openjdk-*; do
                if [ -d "$d" ]; then export JAVA_HOME="$d"; break; fi
            done
        fi
    fi
    if [ -n "$JAVA_HOME" ]; then
        echo "  JAVA_HOME: $JAVA_HOME"
    else
        echo "  [WARN] JAVA_HOME not set — build may fail."
    fi
    export JAVA_HOME
}

# ── Android SDK ───────────────────────────────────────────────────────────
find_tools_android_sdk() {
    if [ -z "$ANDROID_HOME" ]; then
        for dir in "$HOME/Library/Android/sdk" "$HOME/Android/Sdk" /usr/local/lib/android/sdk; do
            if [ -d "$dir" ]; then
                export ANDROID_HOME="$dir"
                break
            fi
        done
    fi
    if [ -z "$ANDROID_HOME" ]; then
        echo ""
        echo "--- Android SDK not found ---"
        if [[ "$(uname)" == "Darwin" ]]; then
            prompt_install "Android SDK" \
                'echo "  Download Android Studio from https://developer.android.com/studio"
                 echo "  After install, SDK is at ~/Library/Android/sdk"' \
                "Install Android Studio and re-run."
        else
            prompt_install "Android SDK" \
                'echo "  Download Android Studio from https://developer.android.com/studio"
                 echo "  Or install via: sudo apt install android-sdk"' \
                "Install Android Studio and re-run."
        fi
        for dir in "$HOME/Library/Android/sdk" "$HOME/Android/Sdk" /usr/local/lib/android/sdk; do
            if [ -d "$dir" ]; then
                export ANDROID_HOME="$dir"
                break
            fi
        done
    fi
    if [ -n "$ANDROID_HOME" ]; then
        echo "  ANDROID_HOME: $ANDROID_HOME"
    else
        echo "  [WARN] ANDROID_HOME not set — build will fail."
    fi
    export ANDROID_HOME
    export ANDROID_SDK="$ANDROID_HOME"
}

# ── NDK ───────────────────────────────────────────────────────────────────
find_tools_ndk() {
    if [ -z "$ANDROID_HOME" ]; then
        find_tools_android_sdk
    fi

    local gradle_file="${SCRIPT_DIR}/../src/app/build.gradle"
    NDK_VERSION=""
    if [ -f "$gradle_file" ]; then
        NDK_VERSION="$(grep 'ndkVersion' "$gradle_file" | grep -oE "[0-9]+\.[0-9]+\.[0-9]+" | head -1)"
    fi
    if [ -z "$NDK_VERSION" ]; then
        NDK_VERSION="27.0.12077973"
    fi

    local ndk_dir="$ANDROID_HOME/ndk/$NDK_VERSION"
    if [ -d "$ndk_dir" ] && ls "$ndk_dir/"* 2>/dev/null | grep -v '\.installer' | head -1 >/dev/null 2>&1; then
        echo "  NDK $NDK_VERSION: OK"
        export NDK_VERSION
        return 0
    fi

    echo "  NDK $NDK_VERSION: not installed"
    local sdkmanager="$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager"

    if [ ! -f "$sdkmanager" ]; then
        echo "  [ERROR] sdkmanager not found at $sdkmanager"
        echo "  Install Android SDK command-line tools and re-run."
        return 1
    fi

    # Clean leftover from previous failed installs
    if [ -d "$ndk_dir" ]; then
        echo "  Cleaning incomplete install at $ndk_dir..."
        rm -rf "$ndk_dir"
    fi

    # Install NDK via sdkmanager (accept license, quiet)
    echo "  Installing NDK $NDK_VERSION via sdkmanager (this may take a while)..."
    yes | "$sdkmanager" --install "ndk;$NDK_VERSION" 2>&1 || true

    # Verify install
    if [ -d "$ndk_dir" ] && ls "$ndk_dir/"* 2>/dev/null | grep -v '\.installer' | head -1 >/dev/null 2>&1; then
        echo "  NDK $NDK_VERSION: installed successfully"
        export NDK_VERSION
        return 0
    fi

    # Retry once if first attempt failed (network/ZIP issues)
    echo "  First attempt failed, retrying..."
    rm -rf "$ndk_dir" 2>/dev/null
    sleep 2
    yes | "$sdkmanager" --install "ndk;$NDK_VERSION" 2>&1 || true

    if [ -d "$ndk_dir" ] && ls "$ndk_dir/"* 2>/dev/null | grep -v '\.installer' | head -1 >/dev/null 2>&1; then
        echo "  NDK $NDK_VERSION: installed successfully"
        export NDK_VERSION
        return 0
    fi

    echo "  [ERROR] Failed to install NDK $NDK_VERSION. Try manually:"
    echo "    $sdkmanager --install 'ndk;$NDK_VERSION'"
    return 1
}

# ── CMake ─────────────────────────────────────────────────────────────────
find_tools_cmake() {
    CMAKE="$(command -v cmake 2>/dev/null || true)"
    if [ -z "$CMAKE" ]; then
        echo ""
        echo "--- CMake not found ---"
        if [[ "$(uname)" == "Darwin" ]]; then
            prompt_install "CMake" \
                'brew install cmake' \
                "Install CMake from https://cmake.org/download/"
        else
            prompt_install "CMake" \
                'sudo apt-get install -y cmake' \
                "Install CMake from https://cmake.org/download/"
        fi
        CMAKE="$(command -v cmake 2>/dev/null || true)"
    fi
    if [ -n "$CMAKE" ]; then
        echo "  cmake: $(cmake --version 2>/dev/null | head -1)"
    fi
    export CMAKE
}

# ── All-in-one ────────────────────────────────────────────────────────────
find_tools_all() {
    find_tools_adb
    find_tools_java
    find_tools_android_sdk
    find_tools_ndk
    find_tools_cmake
    echo ""
    echo "--- Tool summary ---"
    echo "  ADB:          ${ADB:-NOT FOUND}"
    echo "  JAVA_HOME:    ${JAVA_HOME:-NOT FOUND}"
    echo "  ANDROID_HOME: ${ANDROID_HOME:-NOT FOUND}"
    echo "  NDK:          ${NDK_VERSION:-NOT FOUND}"
    echo "  cmake:        ${CMAKE:-NOT FOUND}"
    echo ""
}

# If executed directly (not sourced), run all checks
if [ "${BASH_SOURCE[0]}" = "$0" ]; then
    find_tools_all
fi
