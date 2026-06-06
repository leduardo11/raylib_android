# raylib Android Template

A minimal, working raylib project for Android (and desktop) with a clean game architecture.

## Structure

```
raylib_android/
├── assets/
│   ├── textures/         # PNG/JPG images (bundled into APK)
│   ├── audio/            # WAV/OGG sound effects + music
│   └── fonts/            # TTF custom fonts
├── src/
│   ├── main.cpp          # Entry point: creates Application, sets Splash, runs
│   ├── CMakeLists.txt     # raylib via FetchContent, glbs src/**/*.cpp
│   ├── Core/             # Engine core
│   │   ├── Application   # Owns loop + systems + screen, runs update/render
│   │   ├── GameLoop      # Frame timing, delta time, FPS counter
│   │   ├── Platform      # Platform init (crash handler), asset path
│   │   └── Screen        # Base screen interface (update/render/onEnter/onExit)
│   ├── Systems/          # Subsystems
│   │   ├── Rendering     # Wraps BeginDrawing/EndDrawing, common draw helpers
│   │   ├── Input         # Keyboard, mouse, touch input
│   │   ├── Audio         # Init/close audio device, load/play sounds/music
│   │   └── UI            # Button struct, hit-testing, rendering
│   ├── Screens/          # Game screens (state machine)
│   │   ├── Splash        # 3s splash -> auto-transition to MainMenu
│   │   ├── MainMenu      # Play / Settings / Exit buttons
│   │   └── Game          # Placeholder gameplay screen
│   ├── Game/             # Game-specific logic
│   │   ├── Entities      # Entity manager (create/destroy/query)
│   │   ├── Components    # Transform, Render, Physics structs
│   │   └── Logic         # Game simulation, collision detection
│   ├── AndroidEngine/    # Android-specific helpers
│   │   └── crash_handler/h/cpp  # SIGSEGV/SIGABRT/SIGFPE/SIGILL handler
│   └── app/              # Android Gradle project
│       ├── build.gradle
│       └── src/main/AndroidManifest.xml
└── scripts/
    ├── build_apk.sh      # Build, sign, and verify APK
    ├── clean.sh          # Remove build artifacts
    ├── install_prereqs.sh  # Check/install build prerequisites
    ├── adb_debug.sh      # ADB WiFi pairing + install
    ├── logcat.sh         # View/analyze Android logs
    ├── serve_apk.py      # HTTP server for APK + crash uploads
    └── find_tools.sh     # Common dependency detection
```

### Architecture

```
main.cpp
  └─ Core::Application
       ├─ Core::GameLoop  (frame timing)
       ├─ Systems::Input  (input polling)
       ├─ Systems::Audio  (sound/music)
       └─ Core::Screen*   (current screen)
            ├─ Screens::Splash   -> screens::MainMenu
            ├─ Screens::MainMenu -> Screens::Game | exit
            └─ Screens::Game     -> Screens::MainMenu

Systems::UI / Rendering / Audio / Input are stateless utility structs.
Game::EntityManager + components provide optional ECS-like gameplay layer.
```

## Prerequisites

- **Android Studio** (SDK + NDK) or Android SDK command-line tools
- **JDK 17+** (OpenJDK 17 recommended)
- **CMake** 3.22+ (auto-installed by Gradle if missing)

Install missing tools:
```bash
./scripts/install_prereqs.sh
```

## Build

```bash
./scripts/build_apk.sh
```

The script:
1. Checks prerequisites (JDK, SDK, NDK, CMake)
2. Auto-installs NDK if missing via `sdkmanager`
3. Prunes stale CMake caches to avoid config conflicts
4. Runs Gradle to compile native code and package APK
5. Signs the APK with the debug keystore (auto-created if missing)
6. Shows the final APK path and install instructions

**Output:** `artifacts/outputs/apk/debug/raylib_android-debug.apk`

### Incremental builds

Gradle + Ninja handle incremental builds automatically. Only changed `.cpp` files
are recompiled. No manual cleanup needed between builds.

### Full clean

```bash
./scripts/clean.sh         # Remove all build artifacts
./scripts/clean.sh cxx     # Remove only CMake caches (fixes config issues)
./scripts/clean.sh apk     # Remove only APK files
```

### Desktop build (Linux)

```bash
cmake -B artifacts/linux -S src && cmake --build artifacts/linux -j$(nproc)
./artifacts/linux/raylib_android
```

## Install

### Via ADB (USB/WiFi)

```bash
./scripts/adb_debug.sh install
```

Or directly:
```bash
adb install artifacts/outputs/apk/debug/raylib_android-debug.apk
```

Common install failures:
| Error | Fix |
|---|---|
| `INSTALL_FAILED_UPDATE_INCOMPATIBLE` | `adb uninstall com.raylib.android` then retry |
| `INSTALL_FAILED_NO_MATCHING_ABIS` | Build includes arm64-v8a, armeabi-v7a, x86_64 |
| `App not installed` (no ADB) | Enable "Install unknown apps" for your browser/download manager |

### Via WiFi (no USB)

1. Build the APK: `./scripts/build_apk.sh`
2. Start the server: `./scripts/serve_apk.sh`
3. Open the URL shown (e.g. `http://10.0.0.4:8080/`) on your phone
4. Tap the APK download link and install

## Debugging

### Live logs

```bash
./scripts/logcat.sh              # Watch live logs (raylib_android + stderr)
./scripts/logcat.sh install       # Install APK then watch logs
./scripts/logcat.sh dump 100      # Show last 100 log lines
```

### Crash reports

```bash
./scripts/logcat.sh crash         # Show crash dump
```

After a crash, logs are also saved to `/data/data/com.raylib.android/files/crash_log.txt`
on the device (if the crash handler fires). Retrieve via ADB:
```bash
adb shell cat /data/data/com.raylib.android/files/crash_log.txt
```

### APK verification

```bash
./scripts/logcat.sh verify        # Show APK signature + contents
```

## APK Signing

The build script automatically signs debug APKs with the Android debug keystore
at `~/.android/debug.keystore` (created automatically if missing).

If you need to re-sign manually:
```bash
apksigner sign --ks ~/.android/debug.keystore --ks-pass pass:android \
  --ks-key-alias androiddebugkey --key-pass pass:android \
  --v1-signing-enabled true --v2-signing-enabled true \
  artifacts/outputs/apk/debug/raylib_android-debug.apk
```

Debug keystore credentials:
- **Store password:** `android`
- **Key alias:** `androiddebugkey`
- **Key password:** `android`

## Key Android Gotchas

- **`ANativeActivity_onCreate`**: raylib provides this internally in
  `src/platforms/raylib_android.c`. Linked with `-Wl,--whole-archive` so the
  linker doesn't strip it (CMakeLists.txt handles this).
- **Asset loading**: raylib loads assets directly from the APK via the native
  asset manager. No extraction step needed.
- **Touch input**: raylib maps touch events to mouse events automatically on
  Android. `Systems::Input::touchPos()` uses `GetTouchPosition(0)` (falling
  back to `GetMousePosition()` on desktop).
- **OpenGL ES**: raylib uses OpenGL ES 2.0+ on Android.
- **Entry point**: Define `int main(void)` as usual. raylib's
  `ANativeActivity_onCreate` handles the activity lifecycle and calls `main()`
  after initializing the display.
- **Native-only APK**: No Java/Kotlin code. The manifest has `hasCode="false"`.
  Debug signing is applied manually by `build_apk.sh` after Gradle packaging.
  (AGP 8.x does not sign native-only APKs.)
- **Inline functions in rlgl.h**: raylib 5.5 defines most `rlgl` functions as
  `inline` (without `static`). This can cause "duplicate symbol" linker errors
  if raylib is linked more than once. The template links raylib only once via
  `target_link_libraries`.
