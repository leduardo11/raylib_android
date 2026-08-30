# Proposal: Android Helbreath Client (helbreath_lite) — Starting with an Offline Paperdoll Movement Prototype

**Date:** 2026-08-30
**Repo:** `raylib_android` (this project)
**Server target:** `repos/helbreath_lite` (HelBreath Heldenian)
**Art source:** `repos/cs_rpg` (atlas@2 assets — presentation/pipeline donor only)
**Status:** Proposal v3 (analysis + P0a/P0b/P0c scoping; revised per review: Helbreath-canonical direction, modern joystick quantization, no fixed movement queue, P0a vertical-slice-first).

**Implementation status (2026-08-30):** P0a (vertical slice) is **shipped**, and the
full pre-networking input→wire pipeline is **built and unit-tested** (582 checks,
0 failures). The `PlayerCommand` boundary, `ITargetWorld`/`TargetResolver`,
`GreedyNavigator`/`NavExecutor`, `ProtocolCommand`/`CommandTranslator`,
`HelbreathPacketEncoder`, and the **multitouch HUD** (step 7) are all implemented and
wired into the Game screen. See §10 "Current status & next steps".

---

## 1. Executive summary

This repository is currently a **generic raylib Android/desktop template** — a clean
`Core::Application → Systems → Screens` game loop with ECS-style `Game::Entities`
layers and a working APK build toolchain (`build_apk.sh`, raylib via FetchContent,
ENet-free). The long-term goal is to turn this into a **real Helbreath 3.82 protocol
client** that connects to the open-source `helbreath_lite` server.

The immediate goal of *this proposal* is the simplest possible first milestone that
(a) validates the toolchain on-device and (b) starts mirroring the server's ground
truth. That milestone is a **gameplay-feel prototype**: render an **animated player
character** (reusing `cs_rpg`'s pre-baked atlas@2 art) running around a **simple
offline map** with **8-direction movement on committed tile steps** — using the
**Helbreath direction convention as the canonical simulation model from day one**.
No networking yet — the grid, movement, and direction model mirror `helbreath_lite`
so the sim is already correct before any networking; `cs_rpg` only donates art and a
presentation pipeline that we replace later. We first "test the feeling," then add
server logic.

This document first **analyzes `helbreath_lite`** (what the server actually is and
what a client must speak), cross-references the **`cs_rpg` art/asset pipeline we
borrow (presentation-only)**, then proposes the **offline movement prototype** split
into a brutally small vertical slice plus follow-on slices, and a phased path toward
a full protocol client.

---

## 2. Analysis of `helbreath_lite`

### 2.1 What it is

`helbreath_lite` (HelBreath Heldenian) is an **open-source reimplementation of
Helbreath 3.82** — a C++ server + C++ client + Python bot/test harness. It is the
authoritative engine we want to talk to:

- **Server:** `src/server/` — ENet-only UDP on **port 3000** (ASIO/TCP game path was
  removed in the D10 restructure, 2026-08-29). 14 ENet channels, ~85 message IDs,
  ~110 notification types, ~40 client action types.
- **Client:** `src/client/` — the reference raylib desktop client (`RaylibBackend`),
  which is our **behavioral source of truth** for how the game feels and renders.
- **Shared:** `src/shared/` — single source of truth for packet structs, constants,
  directions, action IDs, view/tile metrics.
- **Persistence:** SQLite (`gamedata.db`, `mapinfo.db`, per-account DBs).
- **Tests:** 50 Python integration suites (`helbot`) + C++ CMake tests (`ctest` 4/4).
- **Assets:** `assets/mapdata/*.amd` (map tile files), `libs/PAK` (sprite packs),
  `assets/{client,server,mapdata,fonts}`.

### 2.2 Architecture map (what a client must understand)

```
helbreath_lite (server, ENet :3000)
  ├─ Login / character select / enter-game  → one ENet session per client
  ├─ CGame message dispatch (msg_process)   → handles CommandMotion, CommandCommon, ...
  ├─ Zone / World (per-map sim, 60 Hz)
  │    └─ CMap tile grid (m_tile[sizeX*sizeY], CTile)
  └─ Composes map content to clients:
       PacketMapData  (init_data + move_map_data): owners, items, dynamic objects
```

**Key fact for a client:** the **static map tile grid never travels over the wire**.
The client already has the `.amd` map files locally and renders tiles itself. The
server only sends *dynamic* entities (players, NPCs, ground items, dynamic objects)
positioned on that locally-rendered grid. So:

- The client ships/loads `.amd` maps (or a derived grid) locally.
- The server confirms/echoes the player's single-tile position and broadcasts nearby
  entity moves.

### 2.3 The movement contract (the thing Phase 0 must reproduce)

All of the following is portable, self-contained, and shared between client/server
already — perfect to port to the Android client before any networking.

**Grid & view (from `src/shared/includes/Net/NetConstants.h`):**
- Tile size **32 px** (`MovementTiming::TILE_SIZE`).
- View frustum **25 × 17 tiles** (`view::TilesX/TilesY`), i.e. 800×600 logical.
- Reference view origin: `PlayerPivotOffsetX/Y = 19/17`, `CenterX/CenterY = 12/9`.
- `view::InitDataTilesX/Y = 25/19` — the tile window the server composes.
- Client map data buffer `MapDataSizeX/Y = 60/55` (client `MapData.h`).

**Directions (`src/shared/includes/Game/DirectionHelpers.h`):**
```
1=N, 2=NE, 3=E, 4=SE, 5=S, 6=SW, 7=W, 8=NW   (0 = none)
OffsetX[8] = {0,  0,1,1,1,0,-1,-1,-1}
OffsetY[8] = {0, -1,-1,0,1,1, 1, 0,-1}
GetNextMoveDir() → 8-direction with asymmetric N/S=3:1 and E/W=4:1 zones.
```

**Action / motion types (`src/shared/includes/Entity/ActionID.h`):**
```
stop=0, Move=1, Run=2, Attack=3, Magic=4, GetItem=5,
Damage=6, DamageMove=7, AttackMove=8, Dying=10
Confirm codes: MoveConfirm=1001, MoveReject=1010, MotionReject=1040
```

**Movement timing (`src/client/includes/EntityMotion.h`):**
- Walk: `WALK_DURATION_MS = 560` (8 frames × 70 ms)
- Run:  `RUN_DURATION_MS  = 312`
- Smooth per-tile interpolation, and the reference client uses a 2-slot pending queue
  for seamless chaining — **we do NOT port that fixed queue into our sim** (see §3/§4:
  live input uses `ActiveStep` + latest `NextMoveIntent`; a queue is left to tap-to-move).

**Client → server (`PacketCommandMotionSimple` / `Attack`, `MsgId::CommandMotion=0x0FA314D5`):**
```
PacketHeader{ msg_id(u32)=0x0FA314D5, msg_type(u16)=action }
int16 x, y            // player's CURRENT tile
uint8 dir             // 1-8
int16 dx, dy          // target offset (attack/direction target)
int16 type
uint32 time_ms        // client timestamp (monotonic counter, not wall-clock!)
```

**Server → client (`MsgId::ResponseMotion=0x0FA314D6`):**
- `MoveConfirm (1001)`: new `x,y` (center-relative), `dir`, `stamina_cost`,
  `occupy_status`, `hp`, then a **map-data delta** (`compose_move_map_data`).
- `MoveReject (1010)`: full player snapshot (object_id, x, y, type, dir, name,
  appearance, status) — client must **bump back / snap** to the authoritative tile.
- `MotionReject (1040)`: `x, y`.

**Server validation (`client_motion_move_handler`, `Game.cpp:423`):**
- `dir` in 1..8, player alive, init-complete.
- The client's reported `sX,sY` **must equal** the server's current `m_x,m_y`
  (else reject code 2).
- Destination `dX,dY = current + ApplyOffset(dir)` must pass `IsTileWalkable(dX,dY)`.

### 2.4 The map / walkability data (for future grid rendering)

- Maps are `.amd` files in `assets/mapdata/` (e.g. `bisle.amd`, `elvine.amd`).
- Server `CMap` (`Map.h`) holds a flat `CTile* m_tile[sizeX*sizeY]`.
- The only grid property that matters for movement is
  `CTile::m_is_move_allowed` (`Tile.h:29`); `Map.cpp:270/276` gating
  `get_is_move_allowed_tile()`. `Map.cpp:484` sets it from map flags.
- For Phase 0, we do **not** need to parse `.amd` — a synthetic walkable grid with a
  spawn tile is enough to prove the direction/step math and the render path. Real
  `.amd` parsing is a later milestone.

### 2.5 Why the current `helbreath_lite` client can't be the Android client

The reference client is desktop raylib + miniaudio + ENet linked natively. Porting
the whole monster `Game.cpp` god class to Android would replay every pre-existing
pain point (PAK sprites currently don't render; login art missing; `Game.cpp` is a
god class by responsibility, per `STATE_AND_ROADMAP.md §3 P4`/`Phase 4`). It is far
cleaner to build a **small, protocol-faithful client from scratch** in this
template, reusing `helbreath_lite`'s shared headers/protocol as the contract rather
than its rendering code. This repo (`raylib_android`) is the right seed: it already
targets Android, is ENet-friendly, and has a clean `Screens` architecture.

### 2.6 What `cs_rpg` gives us (the rendering/feel foundation)

`repos/cs_rpg` is a C# 12 + Raylib tile-based 2D RPG. It is not Helbreath, but it
already solved the exact "character running around an offline map" problem — and its
**assets and pipeline are portable to our C++/raylib client with no runtime coupling.**

**Self-contained atlas@2 assets** (`assets/entities/player/`):
```
player.sheet_00.png   — texture sheet (regions)
player.atlas.json     — region rects
player.pkg.json       — atlas@2 package: clips (per-direction frames + origins),
                        semantic index (role → clip), frameTimeMs, loop, holdLastFrame
```
It is pure JSON + one PNG — no C# needed to consume it. The `AnimatedAsset`
parser (`src/Content/AnimatedAsset.cs`) is data-driven: reads `format=atlas@2`,
`animations[]` (each with `directions[]` of frames referencing regions+origins and
`frameTimeMs`/`loop`/`holdLastFrame`), and `states[]` (semantic → clip + fallback).
We can port this ~60-line parser to C++ almost verbatim.

**Semantic animation model** (`src/Rendering/SpriteSemantics.cs`, `VisualState.cs`):
```
role = locomotion/standing|walking|running | combat/attack|ranged|hit|death | magic/cast | social/pickup|gesture
variant = peace | combat
VisualState.Resolve(locomotion, action?, combatMode) → SpriteSemantic(role, variant) → clip
```
This decouples *state* from *art*: the sim says "walking, south," the renderer picks
the clip. Exactly the seam we want between our movement sim and any future sprites.

**Paperdoll composition** (`PlayerRenderer.cs`): equipment slots → per-direction
bottom→top draw order via `CanonicalEquipmentStack`; the body + each equipped layer
animate in lockstep off a single animator (`Animator`), same semantic clip + frame
index. The renderer only *reads* equipment state; package→item resolution is
upstream. For Phase 0 we draw just the body (and optionally the `barbarian_hammer`
weapon package as a second stacked layer) to prove the compositor.

**Key seam: direction conventions — Helbreath is canonical, art adapts.**
`cs_rpg` uses a different direction convention than `helbreath_lite`:
- `helbreath_lite` **(canonical):** `1=N, 2=NE, 3=E, 4=SE, 5=S, 6=SW, 7=W, 8=NW`
  (wire + sim + input + prediction + replay + server parity all want this).
- `cs_rpg` **(art only):** `Direction` enum `South=0, SouthWest, West, NorthWest,
  North, NorthEast, East, SouthEast` (renderer-facing), with `ToOffset()`/`FaceDirection()`.

The simulation and all gameplay flow use **Helbreath direction from day one**; only
the **presentation layer** translates to cs_rpg's sprite direction when addressing the
atlas. cs_rpg art is a replaceable layer — it disappears when Helbreath sprites land —
so the **permanent simulation model must not be shaped by temporary art**. Concretely:

```
Input
  ↓
HelbreathDirection (N=1 … NW=8)      ← canonical, used everywhere game-side
  ↓
Movement / Grid / Networking         ← prediction, packets, replay, bots all Helbreath
  ↓
SpriteDirectionAdapter               ← presentation-only: Helbreath ↔ cs_rpg sprite dir
  ↓
cs_rpg atlas direction
```

Two clearly-named types enforce the seam: **`HelbreathDirection`** (1-8, the sim +
wire) and **`SpriteDirection`** (cs_rpg renderer-facing), with an explicit adapter in
the presentation layer. Nothing game-side ever sees a `SpriteDirection`.

**`cs_rpg` movement model worth mirroring** (`AGENTS.md`): **committed steps** —
once a step to a destination tile is planted, it cannot change; the entity always
reaches that tile. This is the same guarantee the `helbreath_lite` server enforces
(`client_motion_move_handler`), so a client built on committed steps aligns with the
server naturally and gives solid feeling (no mid-step snap-back). We keep the
committed-step *concept* from cs_rpg but express it in Helbreath terms.

---

## 3. Goals & non-goals for Phase 0

Phase 0 is a **gameplay-feel prototype** answering one question: *Does Helbreath
tile-movement feel right on Android?* It is split into three slices; **only P0a is
required** to answer that question — P0b/P0c are proof slices that follow once P0a
feels good.

### P0a — Vertical slice (required)
A self-contained Android (+ desktop) demo: one player, **animated body sprite**,
**8-direction movement on committed tile steps**, walk/run, a **floating joystick**,
semantic animation, and a centering camera — all on a **simple offline map**. The
simulation uses **`HelbreathDirection` (N=1 … NW=8) as canonical**; art is translated
through a `SpriteDirectionAdapter` presentation-only. The point is to **feel the
movement** — simple map + character running around to validate input, animation, and
tile-step feel before any server work.

### P0b — Rendering proof (secondary)
Port the **atlas@2 package parser**, draw **body + the `barbarian_hammer` weapon**
through a **paperdoll compositor**, and **validate the Helbreath ↔ sprite direction
mapping** against the atlas. Proves the compositor and the direction adapter early.
(Independent of P0a's question but cheap and de-risks later Helbreath sprites.)

### P0c — Navigation (tertiary)
**Tap-to-move** + **BFS pathfinding** + a **path queue** for movement intent.

### Controls (Android) — answer to Q1 "how do ARPGs on mobile work?"
Mobile ARPGs standardize on a **virtual joystick**, not a fixed D-pad:
- **Floating analog joystick** — appears where the thumb touches on the left half of
  the screen, tracks the drag vector. This is the Diablo Immortal / Torchlight /
  Albion pattern and is the most natural for "run toward where I drag."
- **Tap-to-move** (Diablo-lite / Albion, P0c) — tap a tile to plant a path toward it.
- **Right-side action slot** — reserved for the walk↔run toggle and, later,
  attack/skill.

**Joystick → direction (do NOT blindly port `GetNextMoveDir()`).**
`GetNextMoveDir()` is Helbreath **mouse/cursor** directional selection (asymmetric
3:1 / 4:1 zones) — a different device than an analog thumb. The 3:1/4:1 asymmetry is
suspicious for touch and can make diagonals feel weird. Instead:

```
joystick vector
  → dead zone (reject/ignore near-zero input)
  → atan2 angle → octant quantization
  → symmetric 8-way HelbreathDirection
```

We adopt the **modern symmetric** quantization for touch, and **only reconsider
`GetNextMoveDir()` if parity testing proves it captures the directional intent model
we actually want.** The dead zone prevents micro-jitter flip-flopping between
adjacent directions.

### Non-goals (explicitly out of P0a)
- No ENet/TCP networking, no login, no server communication.
- No `.amd` map parsing, no PAK/Helbreath assets — cs_rpg body art + hand-authored
  map for feel only.
- No equipment compositing (that's P0b), no combat/magic/items/NPCs.
- No tap-to-move/pathfinding (that's P0c).
- No full `EntityMotion`/vector interpolation beyond a linear tile lerp.

Everything in Phase 0 is deliberately **offline** so it runs on any device with no
infrastructure; it exists to validate the grid model, input, animation, and rendering
path we will build the networked client on.

---

## 4. Target architecture in this repo

Reuse the existing `raylib_android` template structure, adding a **movement
simulation** (Helbreath-canonical) and a **presentation** layer (cs_rpg art, adapter).
The simulation owns movement and the grid is the spatial authority; the renderer is
read-only and receives interpolated presentation state — it never knows about
committed steps.

```
src/
  main.cpp                      → Core::Application (unchanged)
  Screens/
    GameScreen                   → hosts the World + controls (replaces placeholder Game)
    GridPlay                     (new) — the Phase-0 "running around" screen
  Simulation/                    (new — canonical, Helbreath-canonical, no coupling to art)
    GridCoord.h                  — tile coords, Bounds, Size, Offset
    HelbreathDirection.h         — enum 1=N … 8=NW + OffsetX/OffsetY tables
    MovementTiming.h             — ported WALK_RUN timings from helbreath EntityMotion.h
    MoveIntent.h                 — source-agnostic input (joystick/keyboard/path/net)
    ActiveStep.h                 — the single in-flight committed step; fixed destination
    PlayerMovementSimulation.h   — applies intents → commits steps → publishes PlayerState
    GridWorld.h                  — walkable grid (spatial authority)
  Input/                         (new — producers of MoveIntent; no game state)
    JoystickInput.h/.cpp         — floating analog stick → dead zone → atan2 → 8-way
    KeyboardInput.h/.cpp         — arrows/WASD (desktop dev)
    InputMapper.h                — device input → MoveIntent
  Presentation/                  (new — read-only; ported from cs_rpg; art-only)
    AnimatedAsset.h/.cpp         — atlas@2 .pkg.json parser (port of cs_rpg AnimatedAsset.cs)
    Atlases.h                    — atlas.json region + sheet texture loader
    Animator.h/.cpp              — clip playback (port of cs_rpg Animator.cs)
    SpriteDirectionAdapter.h     — HelbreathDirection ↔ cs_rpg SpriteDirection (presentation-only)
    PlayerRenderer.h/.cpp        — paperdoll compositor (body in P0a; +weapon in P0b;
                                   port of cs_rpg PlayerRenderer.cs)
    Camera.h/.cpp                — centering camera (P0a)
  Navigation/                    (new — P0c; reusable path intent)
    PathFind.h                   — BFS over walkable grid
    PathQueue.h                  — ordered MoveIntent queue (tap-to-move)
  Networking/                    (empty now — later: WireDirection, Protocol, Prediction)
  Systems/Input.cpp              — wire up floating joystick + touch (keep existing touchPos)
  Systems/Rendering.cpp          — add draw-sprite-from-region helper
  assets/entities/player/        — drop in cs_rpg's player + barbarian_hammer packages
```

**Design rules (this is the fixed shape):**
- **`HelbreathDirection` (N=1 … NW=8) is canonical everywhere game-side.** Input
  produces it, movement/grid/replay/networking consume it. Only `Presentation` sees
  `SpriteDirection` (cs_rpg), translated through `SpriteDirectionAdapter`. Art never
  shapes the simulation model.
- **Movement is owned by the simulation; the renderer is read-only.** The state flow:
  ```
  Input → MoveIntent → PlayerMovementSimulation → PlayerState/Snapshot → Renderer
  ```
  `PlayerRenderer` receives interpolated presentation state; it does not reason about
  committed steps.
- **No universal fixed movement queue.** The simulation holds exactly:
  * `ActiveStep` — the current in-flight committed step,
  * `NextMoveIntent` — the latest still-pending desired direction,
  * `PathQueue` *(P0c only)* — ordered intents for tap-to-move/pathfinding.
  A literal FIFO of directions is wrong for live analog input (rotating the stick
  N→NE→E should use the *latest* intent, not replay two stale directions); a queue
  suits tap-to-move, which is why `PathQueue` is a navigation concern, not core.
- Borrowed from the SDK proposal (`helbreath_lite/docs/HELBREATH_SDK_PROPOSAL.md`):
  keep protocol/gameplay constants out of rendering. Simulation constants live in
  `Simulation/`; screens render only what they're told.
- Borrowed from `cs_rpg` (`AGENTS.md` + `PlayerRenderer.cs`): **World is the
  simulation, grid is the spatial authority, renderer is read-only, commands are
  source-agnostic.** We keep these principles but with the *conventions* Helbreath
  dictates.

---

## 5. Phase 0 scope of work

### 5.1 P0a — Vertical slice (the one that answers "does it feel right")

**Grid & view**
1. Port `view::TilesX/Y (25×17)`, `CenterX/Y (12/9)`, `TILE_SIZE (32)` from
   `helbreath_lite` `NetConstants.h`.
2. `GridWorld`: an `int width × int height` walkable mask (+ `GridCoord`,
   `GridBounds`). The spatial authority for movement.
3. Render a small hand-authored map (walkable mask as scene colors; blocked darker).
   Placeholder art for feel, no `.amd`.

**Direction & step math (Helbreath-canonical)**
1. `HelbreathDirection` enum `1=N … 8=NW` + the `OffsetX/OffsetY` tables from
   `DirectionHelpers.h`.
2. Port `WALK_DURATION_MS/RUN_DURATION_MS` from `helbreath_lite` `EntityMotion.h`
   (Walk 560 ms, Run 312 ms).
3. `PlayerMovementSimulation`: takes a `MoveIntent`, plants a **`ActiveStep`**
   (committed — fixed destination), and exposes the latest pending **`NextMoveIntent`**.
   A step commits only if in bounds and walkable. Replaces the earlier 2-slot-queue idea.
4. `SpriteDirectionAdapter`: `HelbreathDirection ↔ cs_rpg SpriteDirection` mapping
   table (presentation-only), verified in P0b.

**Input (Android-first)**
1. **Floating virtual joystick** (`JoystickInput`): thumb down on the left half →
   stick appears → drag vector → **dead zone → atan2 → octant quantization → symmetric
   8-way `HelbreathDirection`**. Release → stop intent. Do NOT port `GetNextMoveDir()`
   unless parity testing later shows it captures the intent model we want.
2. **Walk/Run toggle** button bottom-right; changes step duration only.
3. Desktop **arrow/WASD** (`KeyboardInput`) for dev via existing `Systems::Input`.
4. `InputMapper` converts all device input to a single `MoveIntent` source — the sim
   never cares which device produced it.

**Animation & rendering (P0a body only)**
1. Port the atlas@2 parser (`AnimatedAsset.cs` → `Presentation/AnimatedAsset`) and a
   tiny `Animator` (clip → per-direction frames + `FrameTimeMs`/`Loop`/`HoldLast`,
   delta-driven).
2. Bring in cs_rpg's `assets/entities/player/player.{pkg,atlas}.json` + `sheet_00.png`.
3. `PlayerRenderer`: resolve semantic clip from locomotion (Standing/Walking/Running);
   select the frame by `SpriteDirectionAdapter`; draw the body at pixel position from
   the simulation's **presentation state** (linear lerp across the step duration —
   the renderer does not know about committed steps).
4. `Camera`: center the player tile (start at spawn), clamp to map bounds.

**Documentation** — `docs/phase0-gridplay.md`: ported constants + source paths (both
repos) + the direction mapping table, so the networking pass can cite them.

**Verification (P0a)**
- Desktop build (`cmake -B artifacts/linux -S src && cmake --build ...`) runs: arrows
  + mouse-drag (simulated joystick) run the animated character around the map.
- Android build (`./scripts/build_apk.sh`) installs and runs on-device; floating
  joystick runs the character around.
- Stepping into a blocked/out-of-bounds tile is refused (the `ActiveStep` is rejected),
  mirroring the server's `IsTileWalkable` gate.

### 5.2 P0b — Rendering proof (secondary)
1. **Validate** the atlas@2 C++ parser against `player.pkg.json` (clips/frames match
   the C# source — catch port bugs early).
2. **Validate the `SpriteDirectionAdapter`** by fixing the character at each of the
   8 Helbreath directions and confirming the on-screen facing is correct.
3. **Paperdoll compositor**: draw body + the `barbarian_hammer` weapon package in
   lockstep on a single animator. Prove the composition path before Helbreath sprites
   replace cs_rpg art.

### 5.3 P0c — Navigation (tertiary)
1. **Tap-to-move**: tap a walkable tile.
2. **BFS pathfinding** (`Navigation/PathFind`) over the walkable grid.
3. **`PathQueue`** of ordered `MoveIntent`s replayed as committed steps toward the tap
   target. Unlike live joystick, a queue is the right fit here — it is a navigation
   concern, not core movement. Also the seed of a later click-to-move parity behavior.

---

## 6. Phased roadmap toward the full client

| Phase | Scope | Depends on | Status |
|-------|-------|-----------|--------|
| **P0a** | Vertical slice: animated player, `HelbreathDirection`-canonical committed steps, walk/run, floating joystick, semantic animation, camera — offline, one simple map | — | ✅ shipped |
| P0b | Rendering proof: verify atlas@2 parser + direction adapter; body + weapon compositor | P0a | ✅ atlas@2 parser + camera (compositor later) |
| P0c | Navigation: tap-to-move + BFS + `PathQueue` | P0a | ✅ `GreedyNavigator`/`NavExecutor` (tap-to-move) |
| 1 | Helbreath movement/wire prep: `PlayerCommand` boundary, `WireDirection`, `ITargetWorld`/`TargetResolver` | P0a | ✅ |
| 2 | `ProtocolCommand` + `CommandTranslator` + `HelbreathPacketEncoder` (byte-stable vs hb_lite) | P0a | ✅ (item 5, 6) |
| 2b | **Multitouch HUD** — touch → `PlayerInputFrame` producer, Game screen wiring | P0a | ✅ (item 7) |
| 3 | ENet transport (port `helbreath_lite` ENet client, 1 peer slot, 14 channels) | — | ☐ next |
| 4 | Login / init-data handshake; render server-confirmed player position | P3 | ☐ |
| 5 | Networked movement: send `CommandMotion(Move/Run)`, apply `MoveConfirm`/`MoveReject` (bump on reject), broadcast `EventMotion` for nearby entities | P3, P4 | ☐ |
| 6 | Entity rendering for nearby players/NPCs from `PacketMapData`/`EventMotion` (reuse the Phase-0 renderer) | P5 | ☐ |
| 7 | Helbreath assets: PAK → texture extraction, then Helbreath sprites/paperdoll (replaces cs_rpg art) | — | ☐ |
| 8 | Combat/magic/chat/items over `CommandCommon`/`Notify` | P5 | ☐ |
| 9 | `.amd` map parsing → real walkable/grid data; camera scroll over large maps | — | ☐ |

Each networking phase mirrors the wire structs in `src/shared/includes/Packet/*`
and the server validation in `Game.cpp` — the protocol is the contract; `raylib_android`
owns only the simulation, presentation, and input on top of it. Because the simulation
already speaks `HelbreathDirection`, the networking phases need **no** direction
rework — they send what the sim already produced.

---

## 7. Risks & mitigations

| Risk | Mitigation |
|------|-----------|
| Direction convention mismatch (Helbreath N=1 vs cs_rpg sprite dir) | `HelbreathDirection` is canonical in the sim/game layer; only `SpriteDirectionAdapter` (presentation) translates to cs_rpg art. Never mix conventions outside the adapter. |
| Diagonals feel off on touch joystick | Use modern **symmetric** 8-way quantization (dead zone → atan2 → octants), not a blind `GetNextMoveDir()` port; treat the 3:1/4:1 asymmetry as suspect until parity testing justifies it. |
| Live joystick replaying stale directions (N→NE→E feels laggy) | No fixed movement FIFO. Simulation holds one `ActiveStep` + the latest `NextMoveIntent`; a queue (`PathQueue`) exists only for tap-to-move/navigation. |
| Porting the atlas@2 / paperdoll pipeline to C++ incorrectly | Port the ~60-line `AnimatedAsset` parser near-verbatim; P0b verifier asserts the C++ parser reads `player.pkg.json` and yields the same clips/frames as the C# source. |
| Building Phase 1 before proving Phase 0 | Split Phase 0 into P0a/P0b/P0c; only P0a is required to answer "does it feel right?"; P0b/P0c are gated on sign-off and are proof slices. |
| 25×17 view with 800×600 logical vs varied Android aspects | Define a logical viewport and letterbox/scale to device (as the desktop client does); P0a uses logical 800×600 scaled to fit. |
| Scope creep into networking / real maps | P0a is strictly offline with a placeholder map + cs_rpg art; reject server and `.amd` work until P0a is green and signed-off. |
| cs_rpg assets drift / become unavailable | Copy the needed asset files + JSON into this repo (self-contained), no runtime link to cs_rpg; document the source revision in `docs/phase0-gridplay.md`. |
| ENet not yet ported to Android | Not needed until later; defer. |

---

## 8. Acceptance criteria (Phase 0)

**P0a (required — the vertical slice):**
1. `cmake` desktop build runs: an **animated player** runs around a simple map;
   arrows/WASD + mouse-drag (simulated joystick) move it one tile per step at walk
   and run speeds; animation cycles `standing/walking/running`.
2. `./scripts/build_apk.sh` produces an installable APK; on-device **floating
   joystick** (dead zone → atan2 → symmetric 8-way `HelbreathDirection`) runs the
   character around.
3. The simulation uses **`HelbreathDirection` (N=1 … NW=8) exclusively**; only
   `SpriteDirectionAdapter` (presentation) touches cs_rpg sprite direction.
4. Stepping into a blocked or out-of-bounds tile is refused (the `ActiveStep` is
   rejected).
5. The character tile stays centered and the view never scrolls out of map bounds.
6. `docs/phase0-gridplay.md` documents ported constants and the
   `HelbreathDirection ↔ SpriteDirection` mapping table.

**P0b (rendering proof):**
7. C++ atlas@2 parser yields the same clips/frames as the C# source for
   `player.pkg.json`.
8. Body + `barbarian_hammer` weapon draw in lockstep through the compositor; each of
   the 8 Helbreath directions maps to the correct on-screen facing.

**P0c (navigation):**
9. Tap-to-move BFS paths a character to the tapped tile via an ordered `PathQueue`,
   stopping at blocked tiles or map bounds.

---

## 9. Open questions

1. *(resolved — review)* Canonical direction: **`HelbreathDirection` (N=1 … NW=8) in
   the simulation/input/networking; art adapts** via `SpriteDirectionAdapter`. cs_rpg
   is strictly an asset/presentation donor and never shapes the gameplay model.
2. *(resolved — review)* Joystick input: **dead zone → atan2 → symmetric 8-way
   quantization**; do not port `GetNextMoveDir()` unless parity testing later proves
   it is the intent model we want.
3. *(resolved — review)* Movement core: **`ActiveStep` + latest `NextMoveIntent`**,
   no universal 2-slot queue; `PathQueue` is navigation-only (P0c).
4. *(resolved — review)* Phase 0 split: **P0a vertical slice** (only this answers
   "does it feel right?"), then P0b (rendering/compositor/direction validation),
   then P0c (tap-to-move + BFS).
5. Logical resolution: keep the classic **800×600** / 25×17 view, or a taller viewport
   for modern tall phones? (Recommend: keep 800×600 logical and letterbox for P0a;
   tall-phone framing is a polish decision.)
6. Walk/run pacing: use Helbreath's literal 560 ms / 312 ms, or tune for touch feel
   first and reconcile to Helbreath later? (Recommend: start literal for parity, then
   tune only if feel demands it.)
7. Keep `.amd` parsing out until later, or pull it earlier to use a real `helbreath_lite`
   map for feel? (Recommend: keep synthetic until we must match server walkability
   byte-for-byte.)

---

## 10. Current status & next steps (2026-08-30)

### What is shipped and tested

The offline vertical slice plus the full pre-network input→wire pipeline:

- **P0a** — animated player, `HelbreathDirection`-canonical committed steps,
  walk/run, floating joystick, camera, fallback grid + real map loading.
- **Command boundary** — `PlayerCommand` (six closed variants), `PlayerInputFrame`,
  `InputMapper`/joystick, `TouchFrame`/`KeyState`.
- **World observation** — `ITargetWorld` + `StubTargetWorld` + `GridPlayWorld`
  (app adapter), `TargetResolver`.
- **Navigation** — `GreedyNavigator` + `NavExecutor` driven by the sim's
  committed-step cadence (`beginSingleStep`/`beginStepOpportunity`).
- **Protocol** — `ProtocolCommand`, `CommandTranslator`, `HelbreathPacketEncoder`
  (byte-for-byte vs `helbreath_lite` real packed structs: Motion 21B, MotionAttack
  23B, Common 27B).
- **Item 7 (this session)** — multitouch HUD: left-band floating joystick,
  right-band target reticle (fires on release), NPC context ring, ☰ window menu,
  momentary RUN / separate SUPER, stance toggle, HP/MP hold-repeat, magic slots,
  keyboard parity. Wired into the Game screen; HUD commands route through
  `TargetResolver → NavExecutor → sim` and `CommandTranslator → Encoder` (emit
  + drop, no server yet).
- **Tests** — `tests/gridplay_tests.cpp`: **582 checks, 0 failures** (includes HUD
  producer, resolver, navigator, translator, encoder golden bytes, sim cadence).

Build: `cmake -B artifacts/linux -S src && cmake --build artifacts/linux -j8 --target gridplay_tests && ./artifacts/linux/gridplay_tests`; app target `raylib_android`.

### Next steps (next session)

1. **Phone test the HUD** (needs user): install the APK (build + deploy below), run,
   tap the demo monster (spawn+8,0) to walk-and-attack, tap ground to move, use the
   joystick, SUPER, potions, ☰ menu. Report feel/gesture bugs. The `GRIDPLAY_SHOT`
   env var dumps one frame for off-device review.
2. **Nav tail polish**: `Reached`/`Blocked`/`Stuck` surfacing on the reticle
   (green/red/gray) per the proposal; verify nav resume after manual-input
   suspension.
3. **Real entities instead of the demo list**: hook `GridPlayWorld` to a map-data
   entity source (`PacketMapData` snapshot semantics) so targeting matches real
   content.
4. **ENet transport (roadmap phase 3)**: port `helbreath_lite`'s ENet client
   (1 peer slot, 14 channels); the encoder output is already the exact wire bytes.
5. **Login / init-data handshake (phase 4)**: then networked movement with
   `MoveConfirm`/`MoveReject` and bump-on-reject.
6. Then phases 5–9 per the roadmap table (entity rendering, Helbreath assets,
   combat/magic/items, `.amd` maps).

### Repo rules (unchanged)

- Commit only when asked; keep uncommitted on `develop`.
- Phone testing via the user + `GRIDPLAY_SHOT` (no adb device).
- Desktop build: `cmake -B artifacts/linux -S src && cmake --build artifacts/linux -j8`.
- VPS deploy: `./scripts/deploy_vps.sh` (builds Debug + atomically swaps + verifies).
