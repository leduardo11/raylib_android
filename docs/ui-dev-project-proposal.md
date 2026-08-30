# Proposal: Dedicated Android UI Development Project

## Goal
Create a standalone Android project (`raylib_android_ui`) as a **sibling subdirectory** that isolates the **MobileControlsHud** pipeline for rapid iteration, honing, and testing of the touch UI — without the full game simulation, networking, or asset pipeline overhead.

**Core principle**: The UI project and the main game **share the exact same HUD source** via a `MobileControls/` module. No duplication, no promotion step, no fork risk.

---

## Repository Structure

```
raylib_android/                 # Main game (unchanged)
├── CMakeLists.txt
├── src/
├── assets/
│   ├── art/
│   │   └── hud/               # Frozen assets promoted from UI lab
│   └── ui/
│       └── mobile-controls.json  # Production layout (same artifact UI lab uses)

raylib_android_ui/             # UI development lab (permanent test/tuning host)
├── CMakeLists.txt
├── src/
│   ├── Screens/
│   │   └── Game.cpp/.h        # Minimal: HUD + camera + debug overlay + JSON reload
│   ├── Simulation/
│   │   └── GridPlayWorld.h/.cpp  # Lab's ITargetWorld impl (demo targets)
│   └── DebugOverlay.cpp/.h    # Live params + error overlay + file watcher
├── assets/
│   ├── art/                   # Symlink or copy of ../raylib_android/assets/art/hud/
│   └── ui_layout.json         # Dev override (hot-reloaded)
└── scripts/
    ├── build_ui_apk.sh        # Fast build, no sign (debug only)
    └── watch_layout.sh        # Optional: adb push on file change

shared/
└── MobileControls/            # SHARED MODULE (production library)
    ├── CMakeLists.txt
    ├── Hud/
    │   ├── MobileControlsHud.h/.cpp
    │   ├── MobileControlsHudRender.cpp
    │   └── HudLayout.h/.cpp
    ├── Input/
    │   ├── TouchFrame.h
    │   └── KeyState.h
    ├── Targeting/
    │   ├── TargetWorld.h      # ITargetWorld interface
    │   ├── TargetResolver.h
    │   └── TargetVerb.h
    └── UiLayoutLoader.h/.cpp  # JSON → HudLayout (override, not source)
```

---

## HudLayout: Defaults + JSON Override

```cpp
// shared/MobileControls/Hud/HudLayout.h
struct HudLayout {
    // Canvas
    float logical_w = 1280.0f, logical_h = 720.0f;
    float joystick_zone_w_ratio = 0.70f;

    // Geometry (button rects, ring, etc.)
    struct Rect { float x, y, w, h; };
    Rect menu, attack, run, super, stance, hp, mp, magic1, magic2, magic3;
    Rect window[6];
    float dead_zone = 0.22f, joy_radius = 56.0f;
    float ring_offset = 84.0f, ring_w = 84.0f, ring_h = 40.0f;

    // Behavior constants
    float attack_repeat_s = 0.42f;
    float potion_repeat_s = 0.50f;

    // Colors
    Color reticle_valid, reticle_invalid, nav_target;
    Color monster, item, npc;

    // Asset bindings (keys into asset registry)
    struct Assets {
        std::string attack_btn, run_btn, super_btn, hp_btn, mp_btn;
        std::string magic[3], menu, joystick_base, joystick_handle;
        std::string reticle, ring[4]; // talk, attack, trade, inspect
    } assets;

    static const HudLayout& defaults();          // Hard-coded fallbacks
    bool loadFromJson(const nlohmann::json& j);  // Override with validation
};
```

```cpp
// shared/MobileControls/UiLayoutLoader.cpp
HudLayout UiLayoutLoader::loadOrDefault(const fs::path& jsonPath)
{
    HudLayout layout = HudLayout::defaults();
    if (fs::exists(jsonPath)) {
        try {
            auto json = nlohmann::json::parse(fs::ifstream(jsonPath));
            layout.loadFromJson(json);  // Partial override, preserves defaults
        } catch (...) {
            // Log error, keep defaults
        }
    }
    return layout;
}
```

---

## `mobile-controls.json` — Production Configuration Artifact

The JSON file is the **production configuration artifact**. C++ defaults (`HudLayout::defaults()`) are the **bootstrap/fallback contract** — used when the JSON is absent, malformed, or fails validation. The JSON is a **partial override** over safe defaults.

```json
{
  "version": 1,
  "canvas": { "w": 1280, "h": 720 },
  "layout": {
    "joystick_zone_w_ratio": 0.70,
    "buttons": {
      "menu":   { "x": 1176, "y": 12,  "w": 88,  "h": 44 },
      "attack": { "x": 1088, "y": 500, "w": 192, "h": 180 },
      "run":    { "x": 988,  "y": 448, "w": 88,  "h": 48 },
      "super":  { "x": 1088, "y": 448, "w": 120, "h": 48 },
      "stance": { "x": 1216, "y": 448, "w": 64,  "h": 40 },
      "hp":     { "x": 896,  "y": 648, "w": 86,  "h": 56 },
      "mp":     { "x": 986,  "y": 648, "w": 86,  "h": 56 },
      "magic1": { "x": 896,  "y": 584, "w": 56,  "h": 56 },
      "magic2": { "x": 956,  "y": 584, "w": 56,  "h": 56 },
      "magic3": { "x": 1016, "y": 584, "w": 56, "h": 56 },
      "window": [
        { "x": 1030, "y": 64,  "w": 140, "h": 38 },
        { "x": 1030, "y": 106, "w": 140, "h": 38 },
        { "x": 1030, "y": 148, "w": 140, "h": 38 },
        { "x": 1030, "y": 190, "w": 140, "h": 38 },
        { "x": 1030, "y": 232, "w": 140, "h": 38 },
        { "x": 1030, "y": 274, "w": 140, "h": 38 }
      ],
      "ring": { "offset": 84, "w": 84, "h": 40 }
    }
  },
  "interaction": {
    "dead_zone": 0.22,
    "joy_radius": 56,
    "attack_repeat_s": 0.42,
    "potion_repeat_s": 0.50
  },
  "assets": {
    "attack_btn": "game_dialogs_3.png",
    "run_btn": "equip_models_2.png",
    "super_btn": "game_dialogs_1.png",
    "hp_btn": "item-dynamic_0.png",
    "mp_btn": "item-dynamic_0.png",
    "magic": ["sprite_fonts_0.png", "sprite_fonts_1.png", "sprite_fonts_2.png"],
    "menu": "gamedialog_sprite_1.png",
    "joystick_base": "game_dialogs_0.png",
    "joystick_handle": "game_dialogs_1.png",
    "reticle": "game_dialogs_2.png",
    "ring": ["game_dialogs_4.png", "game_dialogs_5.png", "game_dialogs_6.png", "game_dialogs_7.png"]
  },
  "theme": {
    "reticle_valid": [158, 230, 79, 255],
    "reticle_invalid": [224, 74, 74, 255],
    "nav_target": [79, 200, 230, 255],
    "monster": [224, 90, 90, 255],
    "item": [230, 200, 79, 255],
    "npc": [90, 224, 138, 255]
  }
}
```

---

## Hot Reload (Production-Grade)

`raylib_android_ui` runs a lightweight file watcher on `assets/ui_layout.json`:

```cpp
// DebugOverlay.cpp — runs each frame
void DebugOverlay::pollLayoutReload()
{
    static fs::file_time_type lastWrite = fs::last_write_time(jsonPath);
    auto current = fs::last_write_time(jsonPath);
    if (current <= lastWrite) return;

    // Parse candidate
    HudLayout candidate = HudLayout::defaults();
    bool ok = false;
    try {
        auto json = nlohmann::json::parse(fs::ifstream(jsonPath));
        ok = candidate.loadFromJson(json);
    } catch (...) {}

    if (ok) {
        m_hud.setLayout(std::move(candidate));  // Atomic swap
        lastWrite = current;
        m_reloadError.clear();
    } else {
        m_reloadError = "JSON parse/validation failed — previous layout preserved";
    }
}
```

- **Failed parse/validation**: Previous valid layout preserved, error shown in debug overlay
- **Atomic swap**: `std::atomic<HudLayout*>` or double-buffer
- **No write-back from editor in Week 1–2** — external reload + debug bounds + param overlay is the core value

---

## Build & Iteration Loop

```bash
# One-time setup (builds shared + ui)
cd raylib_android_ui && ./scripts/build_ui_apk.sh

# Dev cycle: edit JSON → push → instant reload
adb push assets/ui_layout.json /sdcard/raylib_ui/ui_layout.json
# UI reloads automatically (file watcher) or tap "Reload" in debug overlay

# Capture frame
GRIDPLAY_SHOT=/sdcard/shot.png ./raylib_android_ui
```

---

## Debug Overlay (Always Visible)

```
[HUD Debug]  frame: 1234  |  touch: 2  |  joy: active(E, 0.78)  |  reticle: (12,8) valid=MONSTER
             |  ring: closed  |  menu: closed  |  nav: suspended  |  atk_target: 10
             |  run: held  |  super: released  |  stance: ON
[Layout]     dead_zone=0.22  attack_repeat=0.42  potion_repeat=0.50
[Touch IDs]  0:(412,308)↓  1:(1134,568)↑
[JSON]       ui_layout.json OK (last reload: 12:34:56)
```

On parse error:
```
[JSON ERROR] ui_layout.json: unexpected token at line 42 — previous layout preserved
```

---

## Milestones

| Week | Deliverable |
|------|-------------|
| 1 | `shared/MobileControls/` module extracted; `UiLayoutLoader` + file watcher + debug overlay; `raylib_android_ui` builds APK < 5 MB |
| 2 | Asset assignment from `assets/art/`; JSON sections (canvas/layout/interaction/assets/theme); external reload working |
| 3 | On-device drag visualization (debug bounds) + param overlay; optional layout editor write-back |
| 4 | Feel validation: dead-zone, RUN hold, SUPER timing, ring ergonomics; freeze layout |
| 5 | Copy `mobile-controls.json` + selected assets → `raylib_android/assets/ui/` + `assets/art/hud/`; delete lab or keep as permanent harness |

---

## Integration Back to Main (Zero Promotion)

When layout is frozen:

1. `cp raylib_android_ui/assets/ui_layout.json raylib_android/assets/ui/mobile-controls.json`
2. `cp -r raylib_android_ui/assets/art/* raylib_android/assets/art/hud/`
3. `raylib_android` CMake: embed `mobile-controls.json` → `UiLayoutLoader::loadOrDefault()` at startup
4. Done. **No C++ changes**, no frozen values in code, no promotion script.

The main game and the UI lab **literally consume the same layout artifact**.

---

## Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| Layout diverges between lab/main | Same `HudLayout` struct, same `UiLayoutLoader`, same JSON artifact |
| Asset mismatch | UI lab symlinks `../raylib_android/assets/art/hud/`; main embeds same files |
| Malformed JSON bricks HUD | `loadOrDefault()` preserves defaults, shows error in overlay |
| Hot reload not instant | File watcher polls every frame (~16ms); adb push latency ~100ms |
| Lab drifts from game behavior | Same `MobileControlsHud` binary; only sim/world/network stubbed in lab |

---

## Decision

Approved. The project is a **sibling subdirectory** with a **shared `MobileControls/` module**. The JSON layout is the **single production artifact** — no C++ freezing, no promotion step, no fork risk. The UI lab becomes a **permanent mobile HUD test harness** that validates the exact same HUD binary the game ships.