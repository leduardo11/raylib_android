#pragma once

#include "Game/Simulation/HelbreathDirection.h"

#include <string>
#include <vector>

namespace Content {

struct AtlasRegion {
    int texture = 0;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

// One sprite-sheet subsection, the draw anchor being `origin` (pivot).
// The sprite is blitted so that `origin == -pivot`: spriteTopLeft = pos - origin.
struct SpriteFrame {
    int region = 0;
    int originX = 0;
    int originY = 0;
};

struct DirectionClip {
    std::string direction;              // "north", "southeast", ...
    std::vector<SpriteFrame> frames;
};

struct AnimClip {
    std::string id;                     // "idle_peace", "run", ...
    int frameTimeMs = 0;
    bool loop = false;
    bool holdLastFrame = false;
    std::vector<DirectionClip> directions;
};

// Semantic index: gameplay role -> animation clip id.
// e.g. {"locomotion/walking", "peace"} -> "walk_peace".
struct SemanticBinding {
    std::string semantic;
    std::string variant;
    std::string animation;
};

// atlas@2 package (cs_rpg / HelbreathAtlasPacker export contract).
struct AtlasV2Package {
    std::string name;
    std::vector<std::string> textures;  // file names, relative to the package dir
    std::vector<AtlasRegion> regions;   // region 0 is the empty placeholder
    std::vector<AnimClip> clips;
    std::vector<SemanticBinding> states;
};

// Parses an atlas@2 .pkg.json document. Throws std::runtime_error on failure.
AtlasV2Package parseAtlasV2(const std::string& jsonText);

// Canonical 8-way direction -> atlas direction name ("north".."northwest").
const char* directionName(Simulation::Direction d);

// Semantic lookup: variant may be empty to match a variant-less binding.
// Returns the resolved clip, or nullptr when nothing matches.
const AnimClip* resolveAnimation(const AtlasV2Package& pkg,
                                 const char* semantic, const char* variant);

// Returns the direction frames for a clip, or nullptr when the clip lacks it.
const DirectionClip* findDirection(const AnimClip& clip,
                                   const char* directionName);

// Frame index at `elapsedMs` into a clip's direction clip. Looping clips wrap;
// non-looping clips with holdLastFrame stay on the final frame; otherwise the
// clock clamps at the last frame. Returns -1 when there are no frames.
int frameIndexAt(const AnimClip& clip, const DirectionClip& direction,
                 float elapsedMs);

} // namespace Content