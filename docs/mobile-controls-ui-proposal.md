# Proposal: Helbreath Mobile Controls & On-Screen UI

**Date:** 2026-08-30
**Repo:** `raylib_android` (this project)
**Source of truth:** `helbreath_lite` `src/client/` (input, hotkeys, HUD, wire ops)
**Status:** Approved architecture, decisions incorporated (v2):
immediate combat tap, NPC context ring, **hold-to-run**, separate
SUPER button, **expandable window menu**, **explicit `SetTarget(target, verb)`**.
Build order unchanged: multitouch is step 7 on top of the command boundary.

---

## 1. Goal

Map every original Helbreath *(HelBreath Heldenian / helbreath_lite)* control —
keyboard + mouse — onto a **touch-only** Android/desktop surface, using the already
agreed architecture:

```
Tap → TargetResolver → SetTarget(target, verb)
   → GreedyNavigator → NavExecutor → next Move (→ Action when reached)
```

and the command boundary:

```
PlayerInputFrame
  PlayerCommand[]  { Move, SetTarget, Attack, Cast, UseItem, ToggleStack }
  ↓
ProtocolCommand
  ↓
HelbreathPacketEncoder  →  ENet
```

Only the final layout, **zoned HUD**, and the **command emission mapping** are
designed here; no multitouch plumbing exists yet (`Systems::Input` is single-pointer).

---

## 2. Inventories of the original control surface

### 2.1 Keys → actions (from `Screen_OnGame.Hotkeys.cpp`, `Game.cpp`, `InputStateHelper.cpp`)

| Input | Action | Wire / side effect |
|---|---|---|
| LMB on ground | walk-to tile (`command_processor` → `set_destination`) | `Type::Move/Run` motion |
| LMB on enemy | walk-to-range then attack (`Type::Attack`, action_type from range) | `make_motion_attack` |
| LMB on self | pickup (interrupts movement, quick-actions) | `Type::GetItem` |
| LMB while moving | re-target immediately | `set_destination` |
| RMB while moving | stop after current step + face click dir (`pending_stop_dir`) | `Type::stop` |
| RMB while stopped | face-click-turn immediately (not during attack/magic anim) | `Type::stop` |
| Shift (held) | run (SP-gated) | converts Move → Run |
| Ctrl+R | toggle persistent run mode | `set_running_mode_enabled` |
| Tab | toggle combat stance / focus cycle | `CommonType::ToggleCombatMode` |
| Shift+Tab | reverse focus cycle | same |
| Home | toggle safe-attack mode | `CommonType::ToggleSafeAttackMode` |
| PageUp | activate special ability | `CommonType::RequestActivateSpecAbility` |
| Ctrl+A | toggle force-attack | local `m_force_attack` |
| Ctrl+D | cycle detail level 0-2 | local `config_manager` |
| Ctrl+S | toggle sound then music | local audio |
| Ctrl+T / Up/Down | whisper target / cycle | chat |
| Ctrl+H / F1 | help window toggle | dialog |
| Ctrl+W | dialog transparency toggle | local |
| Ctrl+X / F10 / F12 | system menu | dialog |
| Ctrl+M | guide map | dialog |
| Ctrl+0..9 | open magic window on circle n | dialog (type=1) |
| F2 / F3 | shortcut slots 2/3 — equip OR cast | `use_shortcut(n)` |
| F4 | cast selected magic (`m_magic_short_cut`) | `magic_casting_system::begin_cast` |
| F5 / F6 / F7 / F8 / F9 | char / inventory / magic / skill / chat windows | dialog |
| Insert (held, 500ms) | HP potion | `CommonType::ReqUseItem` |
| Delete (held, 500ms) | MP potion | `CommonType::ReqUseItem` |
| Esc | cancel chat / dialogs / logout flow | local |
| = / - | map zoom toggle | local config |

### 2.2 Always-on HUD (from `DialogBox_HudPanel`)

- HP/MP gauge bars + numeric, SP bar, EXP bar
- Combat-mode icon (right side)
- 6 toggle icon buttons: **Character, Inventory, Magics, Skills, Chat Log, System Menu**
  (`TOGGLE_BUTTONS[]`, `DialogBox_HudPanel.cpp:21`)

### 2.3 The full moment-to-moment state machine (observed, not theoretical)

The reference client is fundamentally **cursor-driven**: nearly every action is a
*left-click on a world entity or tile*, with **RMB/Shift/Alt as contextual modifiers**
and **F-keys as windowing**. That mental model is the parity target; the mobile port
must reproduce it, not invent a new one where possible:

- Primary finger = **targeting** (what the cursor does)
- Right-side buttons = **modifiers + consumables + casting** (what F-keys/Insert/Delete do)
- Left thumb = **movement** (joystick replaces arrows/RMB-stop-then-turn)

---

## 3. Layout (logical 1280×720, `sensorLandscape`, CAM_ZOOM=2.0)

```
┌──────────────────────────────────────────────────────────────────────────┐
│                                                                    [☰]  │
│                                                                          │
│                                                                          │
│   world view: tap ground = move there                                     │
│               tap enemy  = immediate move-to-range + attack               │
│               tap item   = move-to-range + pickup                         │
│               tap NPC    = select → context ring (Talk/Attack/…)          │
│                                                                          │
│                                                                          │
│  ┌───────────┐        [HP][MP]    ┌────────────┐  [SUPER]                 │
│  │ joystick  │ free-   [ATK slot] │ ⚔ Attack   │  magic F2 F3 F4          │
│  │ left band │ band    [RUN hold] │  hold=rep  │                         │
│  │ 70% width │                    └────────────┘                         │
│  └───────────┘      (right band non-button targets)                      │
└──────────────────────────────────────────────────────────────────────────┘
                  [◀ HP/MP/SP/EXP gauges read-only bottom ▼]
```

**Finger-slots** (multi-touch, raylib `GetTouchPointCount/Position/Id`):

| Slot | Zone | Acts as |
|---|---|---|
| Thumb L | left 70% band | floating joystick → `Move` |
| Thumb R | right 30% band (non-button) | target reticle → `SetTarget(target, verb)` |
| Button presses | any button hit | command, not pointer |

Rules:
1. A touch that lands on a HUD button NEVER becomes a joystick/target action.
2. A touch on the right band shows a **target reticle** at the tile; release fires
   `SetTarget(position, targetId, verb)` — immediate, no second tap for combat.
3. Joystick zone + right-band are disjoint; no arbitration needed beyond
   "button hit wins if it was a button."
4. **☰** MENU (top-right) expands on tap into the six windows, collapses on
   selection or outside-tap (never a permanent six-icon bar).

---

## 4. Command mapping (original → PlayerCommand → ProtocolCommand)

**`SetTarget(target, verb)`** carries the verb explicitly — the resolver never
guesses intent from target type:

```
SetTarget { Position, TargetId?, Verb: Move | Attack | Pickup | Interact }
```

| Original | PlayerCommand | ProtocolCommand → wire |
|---|---|---|
| LMB ground | `SetTarget{tile, Move}` | `Type::Move/Run` via greedy steps |
| LMB enemy | `SetTarget{ent, Attack}` → `Attack{targetId, actionType}` when in range | `Type::Attack` (`make_motion_attack`, target_id, action_type) |
| LMB self / item | `SetTarget{item, Pickup}` → pickup when adjacent | `Type::GetItem` |
| RMB stop+turn | `Move{none}` + `SetTarget{adjacent, Move}` (turn-only) | `Type::stop` + dir |
| Shift (held) | `SetLocomotion{Run}` (momentary, touch: RUN button held) | Move→Run conversion |
| Ctrl+R | `ToggleStack{run}` (persistent, PC-only) | same |
| Tab / Shift+Tab | `ToggleStack{stance}` | `ToggleCombatMode` |
| Home | `ToggleStack{safeAttack}` | `ToggleSafeAttackMode` |
| PageUp | `Cast{specAbility}` | `RequestActivateSpecAbility` |
| Ctrl+A | `ToggleStack{forceAttack}` | local |
| F4 / Ctrl+0..9 | `Cast{magicId}` | `Type::Magic` (`dx=magic_type`) |
| F2 / F3 | `UseItem{slot}` (equip) or `Cast{magicId}` (slot shadow) | `CommonType::ReqUseItem` / short-cut magic |
| Insert / Delete | `UseItem{HP}` / `UseItem{MP}` | `CommonType::ReqUseItem` |
| F5..F10 | `ToggleStack{window}` (UI-only) | none (local dialog) |
| Ctrl+S/D/W | `ToggleStack{sound/detail/transparency}` | none (local) |

---

## 5. Interaction model details

### 5.1 Reticle — immediate combat, context ring for NPCs

Because touch has no hover:
- **Tap enemy** → immediate `SetTarget{ent, Attack}`: greedy-walk into range → `Attack`.
- **Tap ground item** → immediate `SetTarget{item, Pickup}`: walk-to-range → `GetItem`.
- **Tap NPC** → select (reticle + nameplate), a small **context ring** appears:
  Talk / Attack / Trade / Inspect → tap one to fire `SetTarget{ent, Interact|Attack}`.
- **Tap ground** → immediate `SetTarget{tile, Move}`.
- **RMB parity (turn-in-place)** → tap ground adjacent to self = `Move{none}` +
  `SetTarget{adjacent, Move}` → stop + face.

**No two-tap confirmation for ground/enemy targets** — combat must feel immediate.
The explicit verb is carried **on the command** (`SetTarget{..., verb}`), not inferred
by the resolver, so one tap = one unambiguous action. Only multi-verb NPCs get the
ring, and the ring is a choose-a-verb, not a confirm-my-guess.

### 5.2 Attack button (right band)

- **Tap / hold** = normal attack (auto-repeat at the server's swing gate cadence).
- **SUPER** is a **separate momentary modifier button** next to Attack
  (super attack never fires *only* from ATK; it requires the modifier + a valid
  `can_super_attack` target), matching `command_processor`'s Alt-super logic
  (`Game.cpp:4161`, distance thresholds by weapon). Independent button = visible
  affordance, near-impossible accidental supers, and the server-side
  `can_super_attack` validation stays authoritative.
- Attack button disabled when no target entity in `ITargetWorld`.

### 5.3 Modifier buttons (RUN / SUPER / STANCE)

| Button | Momentary or toggle | Maps to |
|---|---|---|
| RUN | **momentary (hold-to-run)** — while held, Move uses `Run`; release → `Walk`. Touch has multitouch, so L-thumb moves while R-thumb holds RUN; no hidden persistent state. (Persistent mode = PC-only `Ctrl+R`, or a future long-press option if playtesting wants it.) | `SetLocomotion{Run\|Walk}` |
| SUPER | momentary (hold opens super, like Alt) | super flag for next `Attack` |
| STANCE (C) | toggle | `ToggleCombatMode` |

### 5.4 Consumables & magic

- 2 slot buttons (HP: `Insert`, MP: `Delete`) — **hold to repeat @500ms**, matching
  the original auto-repeat gate.
- 3 magic shortcut slots (F2/F3/F4 parity): each shows its current binding
  (item equip → potion/etc; magic → spell). Long-press a slot to **re-bind from
  the magic window** (parity with `Ctrl+F2/F3` assign).
- Desktop key parity preserved verbatim (Insert/Delete/F-keys) so PC dev matches
  hb_lite 1:1.

### 5.5 Windows — expandable ☰ menu (not a permanent six-icon bar)

Single **☰ MENU** button, top-right. Tap → fans out the six windows
(`DialogBox_HudPanel::TOGGLE_BUTTONS[]` order): Character / Inventory / Magics /
Skills / Chat Log / System Menu. Select one → open dialog + menu collapses.
Outside-tap collapses. Frequent-actions (HP/MP/combat/CRIT) stay on-screen always;
rare windows hide behind one tap. Chat opens a text input pane (soft keyboard);
gear/SysMenu is Esc equivalence.

### 5.6 Facing niceties

- Release joystick mid-step → player keeps facing last direction (already sim behavior).

---

## 6. GreedyNavigator / NavExecutor seam (no synthetic directions)

`SetTarget` never encodes a direction; it encodes a **position + targetId + verb**.
The navigation seam resolves it:

```
TargetResolver → SetTarget{position, targetId, verb}
   → GreedyNavigator → NextDirectionResult{dir|reached|blocked|stuck}
   → NavExecutor → Move{dir, locomotion}  (one per committed step)
   → Attack / Pickup / Interact when reached
```

- Manual joystick input **suspends** NavExecutor; releasing resumes the tail if the
  target is still valid (agreed earlier).
- `reached/blocked/stuck` surface on the reticle: green (walking), red (blocked),
  gray (stuck/looping) so the player knows why they aren't moving.

---

## 7. Stub seams this proposal depends on (implement same time)

Per agreed implementation order — multitouch UI is LAST, built on top of:

1. `PlayerCommand[]` / `PlayerInputFrame`
2. `ITargetWorld` + `StubTargetWorld`
3. `TargetResolver`
4. `GreedyNavigator` + `NavExecutor`
5. `ProtocolCommand` + `CommandTranslator` tests
6. `HelbreathPacketEncoder`
7. **This multitouch HUD** ← here

The HUD consumes only `PlayerCommand[]` and `ITargetWorld` (for reticle validity /
attack enablement). It never sees wire structs.

---

## 8. Non-goals (for this slice)

- No inventory/equipment window rendering (window *screens* are later slices).
- No soft-keyboard chat yet (pane hook only).
- No pinch-zoom / pan gestures (CAM_ZOOM fixed at 2.0 for now).
- No `.amd` maps / real entities beyond `StubTargetWorld` shapes moved by greedy nav.
- No ENet; `ProtocolCommand` is emitted, tested, and dropped.

---

## 9. Decisions (resolved in review)

| Decision | Verdict |
|---|---|
| Combat tap | Immediate — no two-tap confirm for ground/enemy targets |
| NPC interaction | Reticle + context **ring** (choose a verb, not confirm a guess) |
| RUN | **Momentary hold-to-run** (multitouch frees the R-thumb); no hidden persistent state; persistent = PC-only Ctrl+R or optional later long-press |
| SUPER | Separate momentary button — visible affordance, no accidental supers; server `can_super_attack` stays authoritative |
| Windows | Single **☰** menu expanding to the six windows; collapses on selection/outside-tap |
| No synthetic directions | Kept — joystick and nav never share `handleInput()` |
| HUD → commands only | Kept — HUD consumes only `PlayerCommand[]` + `ITargetWorld` |

**Architectural note:** mobile is simply **another `PlayerInputFrame` producer** next
to keyboard/mouse/network — never a second gameplay implementation.