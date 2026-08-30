#pragma once

// KeyState: engine-agnostic snapshot of the device keys the HUD maps to
// commands, filled by Systems::Input from raylib on desktop. All false on
// Android (no physical keys). "Pressed" = rising edge this frame; "Held" =
// currently down. Pure data so the HUD producer stays testable.

namespace Input {

struct KeyState {
    // Movement (WASD / arrows), held.
    bool moveUp = false;
    bool moveDown = false;
    bool moveLeft = false;
    bool moveRight = false;

    bool shiftHeld = false; // momentary run on keyboard moves
    bool ctrlHeld = false;

    // Toggles / one-shot keys (rising edge).
    bool tabStance = false;    // combat stance
    bool homeSafe = false;     // safe-attack mode
    bool ctrlAForce = false;   // force-attack toggle
    bool ctrlRRun = false;     // persistent run toggle (PC-only parity)
    bool pageUp = false;       // activate spec ability
    bool f2 = false;           // shortcut slot 1
    bool f3 = false;           // shortcut slot 2
    bool f4 = false;           // shortcut slot 3 (cast selected magic)
    bool f5 = false;           // window 0: character
    bool f6 = false;           // window 1: inventory
    bool f7 = false;           // window 2: magics
    bool f8 = false;           // window 3: skills
    bool f9 = false;           // window 4: chat log
    bool f10 = false;          // window 5: system menu

    // Held consumables, repeat @500ms parity (Insert / Delete).
    bool hpInsertHeld = false;
    bool mpDeleteHeld = false;
};

} // namespace Input