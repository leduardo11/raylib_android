// Focused unit tests for the P0a simulation/input logic.
// Header-light: no raylib graphics calls, so it can run as a desktop-only
// console test executable. Invoked by CMakeLists.txt as `gridplay_tests`.

#include <cmath>
#include <cstdio>
#include <string>
#include <type_traits>
#include <variant>

#include "Game/Hud/MobileControlsHud.h"
#include "Game/Input/KeyState.h"
#include "Game/Input/TouchFrame.h"
#include "Game/Input/InputMapper.h"
#include "Game/Input/PlayerCommand.h"
#include "Game/Input/PlayerInputFrame.h"
#include "Game/Protocol/CommandTranslator.h"
#include "Game/Protocol/HelbreathPacketEncoder.h"
#include "Game/Protocol/ProtocolCommand.h"
#include "Game/Simulation/GridWorld.h"
#include "Game/Simulation/GreedyNavigator.h"
#include "Game/Simulation/NavExecutor.h"
#include "Game/Simulation/TargetResolver.h"
#include "Game/Simulation/TargetWorld.h"
#include "Game/Simulation/PlayerMovementSimulation.h"
#include "Game/Input/DirectionQuantizer.h"
#include "Game/Input/JoystickInput.h"
#include "Game/Content/Json.h"
#include "Game/Content/AtlasV2.h"
#include "Game/Content/TileMap.h"
#include "Game/Content/TexturePack.h"

using namespace Simulation;
using namespace Content;

namespace {

int g_checks   = 0;
int g_failures = 0;

#define CHECK(a)                                                           \
    do {                                                                   \
        ++g_checks;                                                        \
        if (!(a)) {                                                        \
            ++g_failures;                                                  \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #a);       \
        }                                                                  \
    } while (0)

template <typename A, typename B>
void printDiff(const char* ea, const char* eb, const A& av, const B& bv)
{
    std::printf("FAIL %s:%d  %s == %s", __FILE__, __LINE__, ea, eb);
    if constexpr (std::is_arithmetic_v<A> && std::is_arithmetic_v<B>)
        std::printf(" (%d vs %d)", (int)av, (int)bv);
    std::printf("\n");
}

#define CHECK_EQ(a, b)                                                   \
    do {                                                                 \
        ++g_checks;                                                      \
        auto va = (a);                                                   \
        auto vb = (b);                                                   \
        if (!(va == vb)) {                                               \
            ++g_failures;                                                \
            printDiff(#a, #b, va, vb);                                   \
        }                                                                \
    } while (0)

GridWorld makeOpenGrid(int w, int h)
{
    GridWorld g(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            g.setWalkable(x, y, true);
    return g;
}

float magnitudeOf(float x, float y) { return sqrtf(x * x + y * y); }

void testDirectionOffsets()
{
    struct Expect { Offset off; Direction d; };
    const Expect table[] = {
        { { 0, -1 }, Direction::N  },
        { { 1, -1 }, Direction::NE },
        { { 1,  0 }, Direction::E  },
        { { 1,  1 }, Direction::SE },
        { { 0,  1 }, Direction::S  },
        { { -1, 1 }, Direction::SW },
        { { -1, 0 }, Direction::W  },
        { { -1, -1 }, Direction::NW },
    };
    for (auto& e : table)
    {
        Offset off = directionOffset(e.d);
        CHECK(off.x == e.off.x);
        CHECK(off.y == e.off.y);
    }
}

void testDirectionQuantizer()
{
    CHECK_EQ(Input::DirectionQuantizer::fromVector( 0.0f, -1.0f), Direction::N);
    CHECK_EQ(Input::DirectionQuantizer::fromVector( 1.0f, -1.0f), Direction::NE);
    CHECK_EQ(Input::DirectionQuantizer::fromVector( 1.0f,  0.0f), Direction::E);
    CHECK_EQ(Input::DirectionQuantizer::fromVector( 1.0f,  1.0f), Direction::SE);
    CHECK_EQ(Input::DirectionQuantizer::fromVector( 0.0f,  1.0f), Direction::S);
    CHECK_EQ(Input::DirectionQuantizer::fromVector(-1.0f,  1.0f), Direction::SW);
    CHECK_EQ(Input::DirectionQuantizer::fromVector(-1.0f,  0.0f), Direction::W);
    CHECK_EQ(Input::DirectionQuantizer::fromVector(-1.0f, -1.0f), Direction::NW);

    // Sector center angles (screen coords: +y is down, so +45 deg = SE).
    CHECK_EQ(Input::DirectionQuantizer::fromVector( cosf(0.0f), sinf( 0.0f)), Direction::E);
    CHECK_EQ(Input::DirectionQuantizer::fromVector( cosf(0.785f), sinf( 0.785f)), Direction::SE);
    CHECK_EQ(Input::DirectionQuantizer::fromVector( 0.0f,  1.0f), Direction::S);
    CHECK_EQ(Input::DirectionQuantizer::fromVector( cosf(4.71f), sinf( 4.71f)), Direction::N);

    // Symmetric equal sectors: E/SE boundary at 22.5 deg, E/NE at -22.5 deg.
    CHECK_EQ(Input::DirectionQuantizer::fromVector( cosf(0.390f), sinf(0.390f)), Direction::E);   // 22.3 deg  -> E
    CHECK_EQ(Input::DirectionQuantizer::fromVector( cosf(0.396f), sinf(0.396f)), Direction::SE);  // 22.7 deg  -> SE
    CHECK_EQ(Input::DirectionQuantizer::fromVector( cosf(-0.390f), sinf(-0.390f)), Direction::E); // -22.3 deg -> E
    CHECK_EQ(Input::DirectionQuantizer::fromVector( cosf(-0.396f), sinf(-0.396f)), Direction::NE);// -22.7 deg -> NE
}

void testDeadZone()
{
    CHECK_EQ(Input::quantizeJoystickVector(0.0f, 0.0f), Direction::S);
    CHECK_EQ(Input::quantizeJoystickVector(0.1f, 0.0f), Direction::S);
    CHECK_EQ(Input::quantizeJoystickVector(0.0f, 0.21f), Direction::S);
    CHECK_EQ(Input::quantizeJoystickVector(0.0f, 0.22f), Direction::S);
    CHECK(Input::quantizeJoystickVector(0.3f, 0.0f) != Direction::S);
    CHECK_EQ(Input::quantizeJoystickVector(0.3f, 0.0f), Direction::E);
}

void testWalkRunDurations()
{
    GridWorld w = makeOpenGrid(10, 10);
    PlayerMovementSimulation sim;
    sim.setWorld(&w);
    sim.setTilePosition(5, 5);

    sim.handleInput(Direction::E, Locomotion::Walking);
    sim.update(1000.0f);
    CHECK(sim.activeStep().active);
    CHECK_EQ(sim.activeStep().durationMs, 560.0f);

    sim.releaseInput();
    sim.update(560.0f);
    CHECK(!sim.activeStep().active);

    sim.handleInput(Direction::E, Locomotion::Running);
    sim.update(1000.0f);
    CHECK(sim.activeStep().active);
    CHECK_EQ(sim.activeStep().durationMs, 312.0f);

    sim.releaseInput();
    sim.update(312.0f);
    CHECK(!sim.activeStep().active);
}

void testStepCannotBeRedirected()
{
    GridWorld w = makeOpenGrid(10, 10);
    PlayerMovementSimulation sim;
    sim.setWorld(&w);
    sim.setTilePosition(5, 5);

    sim.handleInput(Direction::N, Locomotion::Walking);
    sim.update(1000.0f);
    CHECK(sim.activeStep().active);
    CHECK((sim.activeStep().destination == GridCoord{ 5, 4 }));

    // Redirect attempt mid-step must NOT change the committed destination.
    sim.handleInput(Direction::E, Locomotion::Walking);
    sim.update(100.0f);
    CHECK(sim.activeStep().active);
    CHECK((sim.activeStep().destination == GridCoord{ 5, 4 }));
    CHECK(sim.activeStep().direction == Direction::N);

    // Release must not cancel the active step either.
    sim.releaseInput();
    sim.update(600.0f);
    CHECK_EQ(sim.tilePosition().x, 5);
    CHECK_EQ(sim.tilePosition().y, 4);
}

void testIntentsCommitOrder()
{
    // Walk; completes.
    GridWorld w = makeOpenGrid(10, 10);
    PlayerMovementSimulation sim;
    sim.setWorld(&w);
    sim.setTilePosition(5, 5);

    // Latest intent replaces earlier while still moving.
    sim.handleInput(Direction::N, Locomotion::Walking);
    sim.update(1000.0f);
    CHECK((sim.activeStep().destination == GridCoord{ 5, 4 }));

    sim.update(280.0f);                       // mid-step
    sim.handleInput(Direction::E, Locomotion::Walking);
    sim.handleInput(Direction::W, Locomotion::Walking);  // overwrite E
    CHECK(sim.pendingIntent().direction == Direction::W);

    sim.update(560.0f);                       // finish N step -> resolve W
    CHECK(sim.activeStep().active);
    CHECK(sim.activeStep().direction == Direction::W);
    CHECK((sim.activeStep().destination == GridCoord{ 4, 4 }));

    // No input after that: steps freeze where the committed walk ends.
    sim.releaseInput();
    sim.update(620.0f);
    CHECK_EQ(sim.tilePosition().x, 4);
    CHECK_EQ(sim.tilePosition().y, 4);
    CHECK(!sim.activeStep().active);
}

void testRejections()
{
    GridWorld w(10, 10);
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x)
            w.setWalkable(x, y, true);
    w.setWalkable(5, 4, false);               // block the tile north of start

    PlayerMovementSimulation sim;
    sim.setWorld(&w);
    sim.setTilePosition(5, 5);

    // Blocked destination: rejected, no step committed.
    sim.handleInput(Direction::N, Locomotion::Walking);
    sim.update(1000.0f);
    CHECK(!sim.activeStep().active);
    CHECK_EQ(sim.tilePosition().x, 5);
    CHECK_EQ(sim.tilePosition().y, 5);

    // Walkable east instead commits.
    sim.handleInput(Direction::E, Locomotion::Walking);
    sim.update(1000.0f);
    CHECK(sim.activeStep().active);
    sim.releaseInput();
    sim.update(560.0f);
    CHECK(!sim.activeStep().active);

    // Out-of-bounds: top-left corner has no walkable north/west neighbor.
    w.setWalkable(0, 0, true);
    sim.setTilePosition(0, 0);
    sim.handleInput(Direction::N, Locomotion::Walking);
    sim.update(1000.0f);
    CHECK(!sim.activeStep().active);

    sim.handleInput(Direction::W, Locomotion::Walking);
    sim.update(1000.0f);
    CHECK(!sim.activeStep().active);

    CHECK(w.canStepTo(GridCoord{ 9, 4 }));    // cols 0..9, col 9 is walkable
    CHECK(!w.canStepTo(GridCoord{ 10, 4 }));  // out of bounds
    CHECK(!w.canStepTo(GridCoord{ -1, 4 }));  // out of bounds
}

void testInterpolation()
{
    GridWorld w = makeOpenGrid(10, 10);
    PlayerMovementSimulation sim;
    sim.setWorld(&w);
    sim.setTilePosition(5, 5);

    auto p0 = sim.presentation(32.0f);
    CHECK(p0.isMoving == false);
    CHECK(p0.pixelX == 5.5f * 32.0f);
    CHECK(p0.pixelY == 5.5f * 32.0f);

    sim.handleInput(Direction::E, Locomotion::Walking);
    sim.update(1000.0f);                     // commit E step
    sim.update(280.0f);                      // half step (560/2)
    auto p1 = sim.presentation(32.0f);
    CHECK(p1.isMoving);
    CHECK(p1.stepProgress > 0.0f);
    CHECK(p1.pixelX > 5.5f * 32.0f && p1.pixelX < 6.5f * 32.0f);

    // Complete and settle exactly on destination tile.
    sim.releaseInput();
    sim.update(300.0f);
    auto p2 = sim.presentation(32.0f);
    CHECK(!p2.isMoving);
    CHECK((int)(p2.pixelX / 32.0f) == 6);
    CHECK((int)(p2.pixelY / 32.0f) == 5);
}

void testGridWorld()
{
    GridWorld w(10, 10);
    CHECK(w.width() == 10);
    CHECK(w.height() == 10);
    CHECK(!w.isWalkable(0, 0));              // default: all blocked
    CHECK(!w.isWalkable(-1, 0));
    CHECK(!w.isWalkable(0, -1));
    CHECK(!w.isInBounds(10, 0));
    CHECK(!w.isInBounds(0, 10));
    w.setWalkable(2, 2, true);
    CHECK(w.isWalkable(2, 2));
    CHECK(w.canStepTo(GridCoord{ 2, 2 }));
    CHECK(!w.canStepTo(GridCoord{ 2, 3 }));
}

void testJson()
{
    Json::Value root = Json::parse(
        R"({"a":1,"b":[1,2,{"c":"x"}],"d":true,"e":null,"f":{"g":1.5}})");

    CHECK(root.type == Json::Value::Type::Object);
    const Json::Value* a = root.find("a");
    CHECK(a != nullptr);
    CHECK_EQ(a->asInt(), 1);
    CHECK(root.find("missing") == nullptr);

    const Json::Value* b = root.find("b");
    CHECK(b != nullptr);
    CHECK(b->type == Json::Value::Type::Array);
    CHECK_EQ(b->size(), 3);
    const Json::Value* n1 = b->at(1);
    CHECK(n1 != nullptr);
    CHECK_EQ(n1->asInt(), 2);
    const Json::Value* c = b->at(2);
    CHECK(c != nullptr);
    CHECK(c->find("c") != nullptr);
    CHECK_EQ(c->find("c")->asString(), "x");

    CHECK_EQ(root.find("d")->asBool(), true);
    CHECK(root.find("e")->isNull());
    CHECK_EQ(root.find("f")->find("g")->asNumber(), 1.5);

    Json::Value alt = Json::parse("[1,true,\"text\"]");
    CHECK(alt.type == Json::Value::Type::Array);
    CHECK_EQ(alt.at(2)->asString(), "text");

    bool threw = false;
    try { Json::parse("{malformed"); } catch (const std::exception&) { threw = true; }
    CHECK(threw);
}

void testDirectionName()
{
    CHECK_EQ(std::string(Content::directionName(Direction::N)), "north");
    CHECK_EQ(std::string(Content::directionName(Direction::NE)), "northeast");
    CHECK_EQ(std::string(Content::directionName(Direction::E)), "east");
    CHECK_EQ(std::string(Content::directionName(Direction::SE)), "southeast");
    CHECK_EQ(std::string(Content::directionName(Direction::S)), "south");
    CHECK_EQ(std::string(Content::directionName(Direction::SW)), "southwest");
    CHECK_EQ(std::string(Content::directionName(Direction::W)), "west");
    CHECK_EQ(std::string(Content::directionName(Direction::NW)), "northwest");
}

void testAtlasV2()
{
    const std::string pkg =
        R"({
          "format": "atlas@2",
          "kind": "player",
          "name": "t",
          "atlas": {
            "textures": ["t.sheet.png"],
            "regions": [
              { "texture": 0, "x": 0,  "y": 0, "w": 0,  "h": 0 },
              { "texture": 0, "x": 0,  "y": 0, "w": 24, "h": 40 }
            ]
          },
          "animations": [
            {
              "id": "idle_peace", "frameTimeMs": 100, "loop": true,
              "directions": [
                { "direction": "north",
                  "frames": [{ "region": 1, "origin": { "x": 12, "y": 40 } }] },
                { "direction": "east",
                  "frames": [
                    { "region": 1, "origin": { "x": 12, "y": 40 } },
                    { "region": 1, "origin": { "x": 12, "y": 40 } } ] }
              ]
            },
            {
              "id": "die", "frameTimeMs": 100, "loop": false,
              "holdLastFrame": true,
              "directions": [
                { "direction": "south",
                  "frames": [
                    { "region": 1, "origin": { "x": 12, "y": 40 } },
                    { "region": 1, "origin": { "x": 12, "y": 40 } },
                    { "region": 1, "origin": { "x": 12, "y": 40 } } ] }
              ]
            }
          ],
          "states": [
            { "semantic": "locomotion/standing", "variant": "peace",
              "animation": "idle_peace" },
            { "semantic": "locomotion/running",
              "animation": "run" },
            { "semantic": "combat/death", "animation": "die" }
          ]
        })";

    Content::AtlasV2Package parsed = Content::parseAtlasV2(pkg);
    CHECK(parsed.textures.size() == 1);
    CHECK_EQ(parsed.textures[0], "t.sheet.png");
    CHECK(parsed.regions.size() == 2);
    CHECK(parsed.regions[1].w == 24);
    CHECK(parsed.regions[1].h == 40);
    CHECK(parsed.clips.size() == 2);
    CHECK(parsed.states.size() == 3);

    const Content::AnimClip* idle =
        Content::resolveAnimation(parsed, "locomotion/standing", "peace");
    CHECK(idle != nullptr);
    CHECK_EQ(idle->id, "idle_peace");
    CHECK(Content::resolveAnimation(parsed, "locomotion/standing", "combat")
          == nullptr);
    CHECK(Content::resolveAnimation(parsed, "locomotion/running", "")
          == nullptr);
    const Content::AnimClip* die =
        Content::resolveAnimation(parsed, "combat/death", "");
    CHECK(die != nullptr);
    CHECK_EQ(die->id, "die");

    const Content::DirectionClip* east = Content::findDirection(*idle, "east");
    CHECK(east != nullptr);
    CHECK(east->frames.size() == 2);
    CHECK(Content::findDirection(*idle, "west") == nullptr);
    CHECK_EQ(east->frames[0].originX, 12);
    CHECK_EQ(east->frames[0].originY, 40);

    // Looping clip wraps.
    CHECK_EQ(Content::frameIndexAt(*idle, *east, 0.0f), 0);
    CHECK_EQ(Content::frameIndexAt(*idle, *east, 99.9f), 0);
    CHECK_EQ(Content::frameIndexAt(*idle, *east, 100.0f), 1);
    CHECK_EQ(Content::frameIndexAt(*idle, *east, 250.0f), 0);
    CHECK_EQ(Content::frameIndexAt(*idle, *east, 400.0f), 0);
    CHECK_EQ(Content::frameIndexAt(*idle, *east, 500.0f), 1);

    // Non-looping clip holds the last frame.
    const Content::DirectionClip* south = Content::findDirection(*die, "south");
    CHECK(south != nullptr);
    CHECK_EQ(Content::frameIndexAt(*die, *south, 0.0f), 0);
    CHECK_EQ(Content::frameIndexAt(*die, *south, 150.0f), 1);
    CHECK_EQ(Content::frameIndexAt(*die, *south, 250.0f), 2);
    CHECK_EQ(Content::frameIndexAt(*die, *south, 99999.0f), 2);

    bool threw = false;
    try { Content::parseAtlasV2("not json at all"); }
    catch (const std::exception&) { threw = true; }
    CHECK(threw);
}

void testTileMap()
{
    const std::string tilemap =
        R"({"width":3,"height":2,"tileSize":32,
            "layers":[{"type":"grid","name":"ground","tiles":[1,2,0,1,2,2]}]})";
    const std::string atlas =
        R"({"textures":["s.png"],
            "regions":[
              {"texture":0,"x":0,"y":0,"w":0,"h":0},
              {"texture":0,"x":0,"y":0,"w":32,"h":32},
              {"texture":0,"x":32,"y":0,"w":32,"h":32}]})";
    const std::string collision =
        R"({"width":3,"height":2,"cells":[0,1,0,0,1,0]})";
    const std::string manifest = R"({"playerSpawn":{"x":2,"y":1}})";

    Content::TileMapData data =
        Content::parseTileMap(tilemap, atlas, collision, manifest);

    CHECK(data.width == 3);
    CHECK(data.height == 2);
    CHECK(data.tileSize == 32);
    CHECK(data.ground.size() == 6);
    CHECK_EQ(data.ground[0], 1);
    CHECK_EQ(data.ground[2], 0);            // empty tile
    CHECK_EQ(data.ground[3], 1);
    CHECK_EQ(data.ground[5], 2);

    // collision cell == 0 is walkable.
    CHECK(data.walkable.size() == 6);
    CHECK(data.walkable[0]);
    CHECK(!data.walkable[1]);
    CHECK(data.walkable[2]);
    CHECK(data.walkable[3]);
    CHECK(!data.walkable[4]);
    CHECK(data.walkable[5]);

    CHECK(data.textures.size() == 1);
    CHECK_EQ(data.textures[0], "s.png");
    CHECK(data.regions.size() == 3);
    CHECK_EQ(data.regions[2].x, 32);

    CHECK(data.spawnTileX == 2);
    CHECK(data.spawnTileY == 1);

    bool threw = false;
    try { Content::parseTileMap("{}", atlas, collision, manifest); }
    catch (const std::exception&) { threw = true; }
    CHECK(threw);
}

void testSplitPoints()
{
    // Fixed grid cuts: multiples of maxDim, never beyond the sheet size.
    auto small = Content::gridCuts(90, 100);
    CHECK_EQ(small.size(), 2);
    CHECK_EQ(small[0], 0);
    CHECK_EQ(small[1], 90);

    auto med = Content::gridCuts(200, 100);
    CHECK_EQ(med.size(), 3);
    CHECK_EQ(med[0], 0);
    CHECK_EQ(med[1], 100);
    CHECK_EQ(med[2], 200);

    auto big = Content::gridCuts(300, 100);
    CHECK_EQ(big.size(), 4);
    CHECK_EQ(big[1], 100);
    CHECK_EQ(big[2], 200);
    CHECK_EQ(big[3], 300);

    auto flat = Content::gridCuts(4079, 2048);
    CHECK_EQ(flat.size(), 3);
    CHECK_EQ(flat[1], 2048);
    CHECK_EQ(flat[2], 4079);

    // Piece splitting: a region straddling cuts gets one piece per cell.
    Content::AtlasRegion a;  a.texture = 0; a.x = 90; a.y = 95;
                            a.w = 30;      a.h = 30;   // [90,120)x[95,125)
    Content::AtlasRegion b;  b.texture = 0; b.x = 4;  b.y = 4;
                            b.w = 32;      b.h = 32;   // fully inside cell
    Content::AtlasRegion c;  c.texture = 1; c.x = 0;  c.y = 0;
                            c.w = 100;     c.h = 100;  // different sheet
    std::vector<Content::AtlasRegion> regions = { a, b, c };

    auto xs = Content::gridCuts(300, 100);   // 0,100,200,300
    auto ys = Content::gridCuts(300, 100);   // 0,100,200,300
    auto pieces = Content::buildPieces(regions, 0, xs, ys, 3, 0);

    // a straddles x=100 and y=100 -> 4 pieces.
    CHECK_EQ(pieces[0].size(), 4);
    // b is fully inside cell (0,0) -> 1 piece, whole region.
    CHECK_EQ(pieces[1].size(), 1);
    CHECK_EQ(pieces[1][0].texture, 0);
    CHECK_EQ(pieces[1][0].x, 4);
    CHECK_EQ(pieces[1][0].y, 4);
    CHECK_EQ(pieces[1][0].w, 32);
    CHECK_EQ(pieces[1][0].h, 32);
    CHECK_EQ(pieces[1][0].ox, 0);
    CHECK_EQ(pieces[1][0].oy, 0);
    // c is on another texture: no pieces here.
    CHECK_EQ(pieces[2].size(), 0);

    // The pieces of `a` tile back together seamlessly over [0,30)x[0,30).
    int covX0 = 999, covX1 = -1, covY0 = 999, covY1 = -1;
    for (const auto& p : pieces[0])
    {
        CHECK(p.texture >= 0 && p.texture < 9);
        CHECK(p.w > 0 && p.h > 0);
        CHECK(p.x >= 0 && p.y >= 0);
        if (p.ox < covX0) covX0 = p.ox;
        if (p.ox + p.w > covX1) covX1 = p.ox + p.w;
        if (p.oy < covY0) covY0 = p.oy;
        if (p.oy + p.h > covY1) covY1 = p.oy + p.h;
    }
    CHECK_EQ(covX0, 0);
    CHECK_EQ(covY0, 0);
    CHECK_EQ(covX1, 30);       // full region width covered
    CHECK_EQ(covY1, 30);       // full region height covered

    // Exact cell assignment: a = [90,120)x[95,125) in sheet 300 with cuts
    // every 100 -> cells (0,0)=slot0, (0,1)=slot3, (1,0)=slot1, (1,1)=slot4
    // (slot = row*ncols + col, ncols = 3).
    CHECK_EQ(pieces[0][0].texture, 0);   CHECK_EQ(pieces[0][0].x, 90); CHECK_EQ(pieces[0][0].y, 95);
    CHECK_EQ(pieces[0][0].w, 10);        CHECK_EQ(pieces[0][0].h, 5);
    CHECK_EQ(pieces[0][0].ox, 0);        CHECK_EQ(pieces[0][0].oy, 0);

    CHECK_EQ(pieces[0][1].texture, 3);   CHECK_EQ(pieces[0][1].x, 90); CHECK_EQ(pieces[0][1].y, 0);
    CHECK_EQ(pieces[0][1].w, 10);        CHECK_EQ(pieces[0][1].h, 25);
    CHECK_EQ(pieces[0][1].ox, 0);        CHECK_EQ(pieces[0][1].oy, 5);

    CHECK_EQ(pieces[0][2].texture, 1);   CHECK_EQ(pieces[0][2].x, 0);  CHECK_EQ(pieces[0][2].y, 95);
    CHECK_EQ(pieces[0][2].w, 20);        CHECK_EQ(pieces[0][2].h, 5);
    CHECK_EQ(pieces[0][2].ox, 10);       CHECK_EQ(pieces[0][2].oy, 0);

    CHECK_EQ(pieces[0][3].texture, 4);   CHECK_EQ(pieces[0][3].x, 0);  CHECK_EQ(pieces[0][3].y, 0);
    CHECK_EQ(pieces[0][3].w, 20);        CHECK_EQ(pieces[0][3].h, 25);
    CHECK_EQ(pieces[0][3].ox, 10);       CHECK_EQ(pieces[0][3].oy, 5);

    // A second sheet starts its chunk slots right after this one's 9 cells.
    auto pieces2 = Content::buildPieces(regions, 1, xs, ys, 3, 9);
    CHECK_EQ(pieces2[2].size(), 1);          // c = [0,100)x[0,100): one cell
    CHECK_EQ(pieces2[2][0].texture, 9);
    CHECK_EQ(pieces2[2][0].w, 100);
    CHECK_EQ(pieces2[2][0].h, 100);
    CHECK_EQ(pieces2[2][0].ox, 0);
    CHECK_EQ(pieces2[2][0].oy, 0);
}

void testPlayerCommandBoundary()
{
    using Input::CastKind;
    using Input::PlayerAttack;
    using Input::PlayerCast;
    using Input::PlayerCommand;
    using Input::PlayerInputFrame;
    using Input::PlayerMove;
    using Input::PlayerSetTarget;
    using Input::PlayerToggle;
    using Input::PlayerUseItem;
    using Input::TargetVerb;
    using Input::ToggleKind;
    using Input::UseItemSlot;

    // The boundary is exactly the six agreed commands.
    static_assert(Input::playerCommandKindCount == 6,
                  "boundary command count must stay at six");
    static_assert(std::variant_size_v<PlayerCommand> == 6,
                  "boundary command count must stay at six");

    // Move carries direction + locomotion (hold-to-run resolved at the
    // producer edge produces Locomotion::Running directly).
    PlayerCommand c1 = PlayerMove{ Simulation::Direction::NE,
                                   Simulation::Locomotion::Running };
    CHECK(std::holds_alternative<PlayerMove>(c1));
    CHECK_EQ(std::get<PlayerMove>(c1).direction, Simulation::Direction::NE);
    CHECK_EQ(static_cast<int>(std::get<PlayerMove>(c1).locomotion),
             static_cast<int>(Simulation::Locomotion::Running));

    PlayerCommand c1b = PlayerMove{};
    CHECK_EQ(std::get<PlayerMove>(c1b).direction, Simulation::Direction::S);
    CHECK_EQ(static_cast<int>(std::get<PlayerMove>(c1b).locomotion),
             static_cast<int>(Simulation::Locomotion::Walking));

    // SetTarget: entity target carries the id; ground target lets the
    // resolver find it. The verb is explicit, never inferred.
    PlayerCommand c2 = PlayerSetTarget{ { 12, 7 }, 0, false, TargetVerb::Pickup };
    auto& st = std::get<PlayerSetTarget>(c2);
    CHECK_EQ(st.position.x, 12);
    CHECK_EQ(st.position.y, 7);
    CHECK(!st.hasTargetId);
    CHECK_EQ(static_cast<int>(st.verb), static_cast<int>(TargetVerb::Pickup));

    PlayerCommand c2b = PlayerSetTarget{ { 3, 4 }, 9001, true, TargetVerb::Attack };
    auto& stb = std::get<PlayerSetTarget>(c2b);
    CHECK(stb.hasTargetId);
    CHECK_EQ(stb.targetId, 9001u);
    CHECK_EQ(static_cast<int>(stb.verb), static_cast<int>(TargetVerb::Attack));

    // Attack: normal vs SUPER is a modifier on the command.
    PlayerCommand c3 = PlayerAttack{ 77u, true };
    CHECK(std::holds_alternative<PlayerAttack>(c3));
    CHECK_EQ(std::get<PlayerAttack>(c3).targetId, 77u);
    CHECK(std::get<PlayerAttack>(c3).super);

    PlayerCommand c3b = PlayerAttack{ 78u, false };
    CHECK(!std::get<PlayerAttack>(c3b).super);

    // Cast: spec ability is a distinct kind, not a magic spell.
    PlayerCommand c4 = PlayerCast{ 5u, CastKind::SpecAbility };
    CHECK(std::holds_alternative<PlayerCast>(c4));
    CHECK_EQ(std::get<PlayerCast>(c4).magicId, 5u);
    CHECK_EQ(static_cast<int>(std::get<PlayerCast>(c4).kind),
             static_cast<int>(CastKind::SpecAbility));

    // UseItem slots: named, not magic numbers.
    PlayerCommand c5 = PlayerUseItem{ UseItemSlot::Hp };
    CHECK(std::holds_alternative<PlayerUseItem>(c5));
    CHECK_EQ(static_cast<int>(std::get<PlayerUseItem>(c5).slot),
             static_cast<int>(UseItemSlot::Hp));
    CHECK_EQ(static_cast<int>(UseItemSlot::Mp), 4);

    // ToggleStack: momentary run uses on=false on release; Window carries id.
    PlayerCommand c6 = PlayerToggle{ ToggleKind::Run, false, 0 };
    CHECK(std::holds_alternative<PlayerToggle>(c6));
    CHECK_EQ(static_cast<int>(std::get<PlayerToggle>(c6).kind),
             static_cast<int>(ToggleKind::Run));
    CHECK(!std::get<PlayerToggle>(c6).on);

    PlayerCommand c7 = PlayerToggle{ ToggleKind::Window, true, 2 };
    CHECK_EQ(static_cast<int>(std::get<PlayerToggle>(c7).windowId), 2);

    // Frame: fixed capacity, ordered push, overflow drops without growing.
    PlayerInputFrame f;
    f.frameIndex = 42;
    CHECK_EQ(f.count(), 0);
    CHECK(f.push(PlayerMove{ Simulation::Direction::E,
                             Simulation::Locomotion::Walking }));
    CHECK(f.push(PlayerSetTarget{ { 1, 1 }, 0, false, TargetVerb::Move }));
    CHECK_EQ(f.count(), 2);

    int iterated = 0;
    int kindSum = 0;
    for (const auto& cmd : f)
    {
        ++iterated;
        kindSum += static_cast<int>(cmd.index());
    }
    CHECK_EQ(iterated, 2);
    CHECK_EQ(kindSum, 1); // indices 0 (Move) + 1 (SetTarget)

    f.clear();
    CHECK_EQ(f.count(), 0);

    int pushed = 0;
    for (int i = 0; i < Input::kPlayerCommandsPerFrame; ++i)
    {
        CHECK(f.push(PlayerAttack{ (uint32_t)i, false }));
        ++pushed;
    }
    CHECK_EQ(pushed, Input::kPlayerCommandsPerFrame);
    CHECK_EQ(f.count(), Input::kPlayerCommandsPerFrame);
    CHECK(!f.push(PlayerAttack{ 1u, false })); // full -> drop, no grow
    CHECK_EQ(f.count(), Input::kPlayerCommandsPerFrame);
}

void testTargetWorldStub()
{
    using Input::TargetVerb;
    using Simulation::GridCoord;
    using Simulation::StubTargetWorld;
    using Simulation::TargetInfo;
    using Simulation::TargetKind;

    StubTargetWorld w(8, 8, { 2, 2 });

    TargetInfo monster{ 101, { 4, 2 }, TargetKind::Monster, true,  false, false };
    TargetInfo item   { 202, { 5, 5 }, TargetKind::Item,    false, true,  false };
    TargetInfo npc    { 303, { 1, 6 }, TargetKind::Npc,     false, false, true };
    w.add(monster).add(item).add(npc);

    // Player state: position + self detection.
    CHECK_EQ(w.playerPosition().x, 2);
    CHECK_EQ(w.playerPosition().y, 2);
    CHECK(w.isPlayerTargetId(1u));        // default player id
    CHECK(!w.isPlayerTargetId(101u));
    w.setPlayerId(101u);
    CHECK(w.isPlayerTargetId(101u));
    CHECK(!w.isPlayerTargetId(1u));

    // Existence + location + kind + capabilities.
    TargetInfo out;
    CHECK(w.tryGetTarget(101u, out));
    CHECK_EQ(out.id, 101u);
    CHECK_EQ(out.position.x, 4);
    CHECK_EQ(out.position.y, 2);
    CHECK_EQ(static_cast<int>(out.kind), static_cast<int>(TargetKind::Monster));
    CHECK(out.attackable);
    CHECK(!out.pickupable);
    CHECK(!w.tryGetTarget(404u, out));

    // Find by tile; first inserted wins on a tie (deterministic).
    CHECK(w.findTargetAt({ 5, 5 }, out));
    CHECK_EQ(out.id, 202u);
    w.add(TargetInfo{ 404, { 5, 5 }, TargetKind::Monster, true, false, false });
    CHECK(w.findTargetAt({ 5, 5 }, out));
    CHECK_EQ(out.id, 202u);
    CHECK(!w.findTargetAt({ 3, 3 }, out));

    // Walkability + bounds.
    w.setWalkable(3, 3, false);
    CHECK(!w.isTileValid({ 3, 3 }));
    CHECK(w.isTileValid({ 2, 2 }));
    CHECK(!w.isTileValid({ 10, 2 }));     // out of bounds
    CHECK(!w.isTileValid({ 2, 10 }));

    // Per-verb interaction ranges: defaults + overrides.
    CHECK_EQ(w.interactionRange(TargetVerb::Attack, monster), 1);
    w.setRange(TargetVerb::Attack, 2);
    CHECK_EQ(w.interactionRange(TargetVerb::Attack, monster), 2);
    CHECK_EQ(w.interactionRange(TargetVerb::Pickup, item), 1);
    CHECK_EQ(w.interactionRange(TargetVerb::Interact, npc), 1);
    CHECK_EQ(w.interactionRange(TargetVerb::Move, npc), 0);

    // TargetVerb vocabulary is shared and value-stable.
    CHECK_EQ(static_cast<int>(TargetVerb::Move), 0);
    CHECK_EQ(static_cast<int>(TargetVerb::Attack), 1);
    CHECK_EQ(static_cast<int>(TargetVerb::Pickup), 2);
    CHECK_EQ(static_cast<int>(TargetVerb::Interact), 3);
}

void testTargetResolver()
{
    using Input::TargetVerb;
    using Simulation::GridCoord;
    using Simulation::ResolvedTarget;
    using Simulation::StubTargetWorld;
    using Simulation::TargetInfo;
    using Simulation::TargetKind;
    using Simulation::TargetResolver;

    // 8x6 world, player at (1,1). Wall at (3,3).
    StubTargetWorld w(8, 6, { 1, 1 });
    w.setRange(TargetVerb::Attack, 2);
    w.setRange(TargetVerb::Pickup, 1);
    w.setRange(TargetVerb::Interact, 1);
    w.add({ 10, { 5, 2 }, TargetKind::Monster, true,  false, false }); // attacker
    w.add({ 20, { 5, 4 }, TargetKind::Item,    false, true,  false }); // item
    w.add({ 30, { 2, 1 }, TargetKind::Npc,     false, true,  true });  // talkable npc
    w.setWalkable(3, 3, false);

    // Ground move: valid tile -> destination, Move, no target, range 0.
    auto r = TargetResolver::resolve(w, { { 6, 4 }, 0, false, TargetVerb::Move });
    CHECK(r.has_value());
    if (!r) return;
    CHECK(!r->hasTargetId);
    CHECK_EQ(r->destination.x, 6);
    CHECK_EQ(r->destination.y, 4);
    CHECK_EQ(static_cast<int>(r->verb), static_cast<int>(TargetVerb::Move));
    CHECK_EQ(r->approachRange, 0);

    // Ground move onto a wall: not resolvable.
    auto rWall = TargetResolver::resolve(w, { { 3, 3 }, 0, false, TargetVerb::Move });
    CHECK(!rWall);

    // Attack by id: fresh anchor = entity's current position, world range.
    auto rId = TargetResolver::resolve(w, { { 9, 9 }, 10, true, TargetVerb::Attack });
    CHECK(rId.has_value());
    if (!rId) return;
    CHECK(rId->hasTargetId);
    CHECK_EQ(rId->targetId, 10u);
    CHECK_EQ(rId->destination.x, 5);
    CHECK_EQ(rId->destination.y, 2);
    CHECK_EQ(static_cast<int>(rId->verb), static_cast<int>(TargetVerb::Attack));
    CHECK_EQ(rId->approachRange, 2);

    // Attack by position (no id): located via the world.
    auto rPos = TargetResolver::resolve(w, { { 5, 2 }, 0, false, TargetVerb::Attack });
    CHECK(rPos.has_value());
    if (!rPos) return;
    CHECK_EQ(rPos->targetId, 10u);
    CHECK(rPos->hasTargetId);
    CHECK_EQ(rPos->approachRange, 2);

    // Pickup by position; interact by position.
    auto rPick = TargetResolver::resolve(w, { { 5, 4 }, 0, false, TargetVerb::Pickup });
    CHECK(rPick.has_value());
    if (!rPick) return;
    CHECK_EQ(rPick->targetId, 20u);
    CHECK_EQ(rPick->approachRange, 1);

    auto rTalk = TargetResolver::resolve(w, { { 2, 1 }, 0, false, TargetVerb::Interact });
    CHECK(rTalk.has_value());
    if (!rTalk) return;
    CHECK_EQ(rTalk->targetId, 30u);
    CHECK_EQ(rTalk->approachRange, 1);

    // Capability mismatch: attack intent on a non-attackable item falls back
    // to Move (walk up), no target, range 0.
    auto rBad = TargetResolver::resolve(w, { { 5, 4 }, 0, false, TargetVerb::Attack });
    CHECK(rBad.has_value());
    if (!rBad) return;
    CHECK(!rBad->hasTargetId);
    CHECK_EQ(static_cast<int>(rBad->verb), static_cast<int>(TargetVerb::Move));
    CHECK_EQ(rBad->destination.x, 5);
    CHECK_EQ(rBad->destination.y, 4);
    CHECK_EQ(rBad->approachRange, 0);

    // Attack intent on empty ground: same Move fallback.
    auto rEmpty = TargetResolver::resolve(w, { { 6, 0 }, 0, false, TargetVerb::Attack });
    CHECK(rEmpty.has_value());
    if (!rEmpty) return;
    CHECK_EQ(static_cast<int>(rEmpty->verb), static_cast<int>(TargetVerb::Move));

    // Attack intent on a wall tile: fallback Move is rejected too.
    auto rWallAttack = TargetResolver::resolve(w, { { 3, 3 }, 0, false, TargetVerb::Attack });
    CHECK(!rWallAttack);

    // Targeting the player: not resolvable (no attacking yourself).
    w.setPlayerId(10u);
    auto rSelf = TargetResolver::resolve(w, { { 5, 2 }, 10, true, TargetVerb::Attack });
    CHECK(!rSelf);

    // Moving-target anchor: resolve by id against a world where the entity has
    // drifted from the tapped tile — destination follows the current position.
    StubTargetWorld w2(8, 6, { 1, 1 });
    w2.setRange(TargetVerb::Attack, 2);
    w2.add({ 10, { 7, 2 }, TargetKind::Monster, true, false, false });
    auto rDrift = TargetResolver::resolve(w2, { { 5, 2 }, 10, true, TargetVerb::Attack });
    CHECK(rDrift.has_value());
    if (!rDrift) return;
    CHECK_EQ(rDrift->destination.x, 7);
    CHECK_EQ(rDrift->destination.y, 2);

    // Player standing on their own item pickups immediately (range already met).
    StubTargetWorld w3(8, 6, { 4, 4 });
    w3.setRange(TargetVerb::Pickup, 1);
    w3.add({ 50, { 4, 4 }, TargetKind::Item, false, true, false });
    auto rHere = TargetResolver::resolve(w3, { { 4, 4 }, 0, false, TargetVerb::Pickup });
    CHECK(rHere.has_value());
    if (!rHere) return;
    CHECK_EQ(rHere->targetId, 50u);
    CHECK_EQ(rHere->approachRange, 1);
}

void testGreedyNavigator()
{
    using Input::TargetVerb;
    using Simulation::Direction;
    using Simulation::GreedyNavigator;
    using Simulation::GridCoord;
    using Simulation::ResolvedTarget;
    using Simulation::StubTargetWorld;

    StubTargetWorld w(20, 20, { 1, 1 });

    GreedyNavigator nav;
    CHECK(!nav.hasTarget());
    auto idle = nav.next(w, { 2, 2 });
    CHECK_EQ(static_cast<int>(idle.status), static_cast<int>(GreedyNavigator::Status::Idle));

    // Straight east, no detours.
    nav.setTarget(ResolvedTarget{ 0, false, { 7, 2 }, TargetVerb::Move, 0 });
    GridCoord from = { 2, 2 };
    int steps = 0;
    for (int i = 0; i < 16 && from.x != 7; ++i)
    {
        auto r = nav.next(w, from);
        CHECK_EQ(static_cast<int>(r.status), static_cast<int>(GreedyNavigator::Status::Moving));
        CHECK_EQ(static_cast<int>(r.direction), static_cast<int>(Direction::E));
        from = from + directionOffset(r.direction);
        ++steps;
    }
    CHECK_EQ(steps, 5);
    auto reached = nav.next(w, from);
    CHECK_EQ(static_cast<int>(reached.status), static_cast<int>(GreedyNavigator::Status::Reached));

    // Diagonal greedy: target (5,4) from (2,2) -> SE, SE, E. Deterministic and
    // monotone (no zigzag).
    nav.setTarget(ResolvedTarget{ 0, false, { 5, 4 }, TargetVerb::Move, 0 });
    from = { 2, 2 };
    int se = 0, e = 0, total = 0;
    for (int i = 0; i < 8 && !(from.x == 5 && from.y == 4); ++i)
    {
        auto r = nav.next(w, from);
        CHECK_EQ(static_cast<int>(r.status), static_cast<int>(GreedyNavigator::Status::Moving));
        if (r.direction == Direction::SE) ++se;
        else if (r.direction == Direction::E) ++e;
        from = from + directionOffset(r.direction);
        ++total;
    }
    CHECK_EQ(se, 2);
    CHECK_EQ(e, 1);
    CHECK_EQ(total, 3);
    CHECK_EQ(from.x, 5);
    CHECK_EQ(from.y, 4);

    // Within attack range already -> immediate Reached, no step.
    nav.setTarget(ResolvedTarget{ 0, false, { 5, 2 }, TargetVerb::Attack, 2 });
    auto inRange = nav.next(w, { 3, 3 });
    CHECK_EQ(static_cast<int>(inRange.status), static_cast<int>(GreedyNavigator::Status::Reached));

    // Detour: wall at (3,3) forces E before SE; first step Blocked, then clean.
    w.setWalkable(3, 3, false);
    nav.setTarget(ResolvedTarget{ 0, false, { 4, 3 }, TargetVerb::Move, 0 });
    auto d1 = nav.next(w, { 2, 2 });
    CHECK_EQ(static_cast<int>(d1.status), static_cast<int>(GreedyNavigator::Status::Blocked));
    CHECK_EQ(static_cast<int>(d1.direction), static_cast<int>(Direction::E));
    auto d2 = nav.next(w, { 3, 2 });
    CHECK_EQ(static_cast<int>(d2.status), static_cast<int>(GreedyNavigator::Status::Moving));
    CHECK_EQ(static_cast<int>(d2.direction), static_cast<int>(Direction::SE));
    auto d3 = nav.next(w, { 4, 3 });
    CHECK_EQ(static_cast<int>(d3.status), static_cast<int>(GreedyNavigator::Status::Reached));

    // Dead end: forward walled off, only the retrace tile open -> Stuck, and the
    // retrace tile is never emitted.
    StubTargetWorld w2(10, 10, { 3, 2 });
    w2.setWalkable(4, 2, false);
    w2.setWalkable(4, 1, false);
    w2.setWalkable(4, 3, false);
    nav.setTarget(ResolvedTarget{ 0, false, { 6, 2 }, TargetVerb::Move, 0 });
    auto stuck = nav.next(w2, { 3, 2 });
    CHECK_EQ(static_cast<int>(stuck.status), static_cast<int>(GreedyNavigator::Status::Stuck));
}

void testNavExecutor()
{
    using Input::PlayerMove;
    using Input::TargetVerb;
    using Simulation::Direction;
    using Simulation::GridCoord;
    using Simulation::Locomotion;
    using Simulation::NavExecutor;
    using Simulation::ResolvedTarget;
    using Simulation::StubTargetWorld;
    using Simulation::TargetInfo;
    using Simulation::TargetKind;
    using Simulation::TargetResolver;
    using Simulation::toMoveIntent;

    StubTargetWorld w(20, 20, { 2, 2 });
    w.setRange(TargetVerb::Attack, 2);
    w.add({ 10, { 5, 4 }, TargetKind::Monster, true, false, false });

    // Full tap-enemy run: greedy walk into range, then exactly one Attack.
    auto resolved = TargetResolver::resolve(w, { { 5, 4 }, 0, false, TargetVerb::Attack });
    CHECK(resolved.has_value());
    if (!resolved) return;

    NavExecutor ex;
    CHECK(!ex.isEngaged());
    ex.engage(*resolved);
    CHECK(ex.isEngaged());
    CHECK_EQ(static_cast<int>(ex.status()),
             static_cast<int>(Simulation::GreedyNavigator::Status::Idle));

    GridCoord from = { 2, 2 };
    int moves = 0;
    for (int i = 0; i < 16; ++i)
    {
        auto r = ex.nextMove(w, from);
        if (r.move)
        {
            ++moves;
            from = from + directionOffset(r.move->direction);
        }
        if (r.action)
        {
            CHECK(std::holds_alternative<Input::PlayerAttack>(*r.action));
            auto atk = std::get<Input::PlayerAttack>(*r.action);
            CHECK_EQ(atk.targetId, 10u);
            CHECK(!atk.super);
            break;
        }
    }
    CHECK_EQ(moves, 1); // SE to (3,3): within range 2 of (5,4) -> reach
    CHECK_EQ(from.x, 3);
    CHECK_EQ(from.y, 3);
    CHECK_EQ(static_cast<int>(ex.status()),
             static_cast<int>(Simulation::GreedyNavigator::Status::Reached));

    // The reach-action is one-shot: a second steady-state call emits nothing.
    auto after = ex.nextMove(w, from);
    CHECK(!after.move);
    CHECK(!after.action);

    // Locomotion flows into emitted moves (hold-to-run producer contract).
    NavExecutor ex2;
    auto moveTarget = TargetResolver::resolve(w, { { 9, 2 }, 0, false, TargetVerb::Move });
    CHECK(moveTarget.has_value());
    if (!moveTarget) return;
    ex2.engage(*moveTarget);
    ex2.setLocomotion(Locomotion::Running);
    auto r1 = ex2.nextMove(w, { 2, 2 });
    CHECK(r1.move.has_value());
    CHECK_EQ(static_cast<int>(r1.move->locomotion),
             static_cast<int>(Locomotion::Running));
    ex2.setLocomotion(Locomotion::Walking);
    auto r2 = ex2.nextMove(w, { 3, 2 });
    CHECK(r2.move.has_value());
    CHECK_EQ(static_cast<int>(r2.move->locomotion),
             static_cast<int>(Locomotion::Walking));

    // Suspend: no emits, target kept; resume re-arms and continues.
    NavExecutor ex3;
    ex3.engage(*moveTarget);
    CHECK(ex3.nextMove(w, { 2, 2 }).move.has_value());
    ex3.setSuspended(true);
    CHECK(ex3.isSuspended());
    auto suspended = ex3.nextMove(w, { 3, 2 });
    CHECK(!suspended.move);
    CHECK_EQ(static_cast<int>(suspended.status),
             static_cast<int>(Simulation::GreedyNavigator::Status::Idle));
    ex3.setSuspended(false);
    auto resumed = ex3.nextMove(w, { 3, 2 });
    CHECK(resumed.move.has_value());

    // Disengage stops all output and clears the target.
    ex3.disengage();
    CHECK(!ex3.isEngaged());
    CHECK(!ex3.nextMove(w, { 3, 2 }).move);

    // toMoveIntent bridges command vocabulary to the sim's MoveIntent.
    auto mi = toMoveIntent(PlayerMove{ Direction::E, Locomotion::Running });
    CHECK_EQ(static_cast<int>(mi.direction), static_cast<int>(Direction::E));
    CHECK_EQ(static_cast<int>(mi.locomotion), static_cast<int>(Locomotion::Running));
    CHECK(mi.active);
}

void testCommandTranslator()
{
    using Input::CastKind;
    using Input::PlayerAttack;
    using Input::PlayerCast;
    using Input::PlayerCommand;
    using Input::PlayerMove;
    using Input::PlayerSetTarget;
    using Input::PlayerToggle;
    using Input::PlayerUseItem;
    using Input::TargetVerb;
    using Input::ToggleKind;
    using Input::UseItemSlot;
    using Protocol::ActionType;
    using Protocol::Common;
    using Protocol::CommonType;
    using Protocol::CommandTranslator;
    using Protocol::IWireContext;
    using Protocol::Motion;
    using Protocol::MotionAttack;
    using Protocol::ShortcutBinding;
    using Simulation::GridCoord;
    using Simulation::Locomotion;
    using Simulation::StubTargetWorld;
    using Simulation::TargetInfo;
    using Simulation::TargetKind;

    // Wire-id stability (hb_lite ActionID.h / NetMessages.h).
    CHECK_EQ(static_cast<int>(ActionType::Stop), 0);
    CHECK_EQ(static_cast<int>(ActionType::Move), 1);
    CHECK_EQ(static_cast<int>(ActionType::Run), 2);
    CHECK_EQ(static_cast<int>(ActionType::Attack), 3);
    CHECK_EQ(static_cast<int>(ActionType::Magic), 4);
    CHECK_EQ(static_cast<int>(ActionType::GetItem), 5);
    CHECK_EQ(static_cast<int>(ActionType::AttackMove), 8);
    CHECK_EQ(static_cast<int>(CommonType::ToggleCombatMode), 0x0A0B);
    CHECK_EQ(static_cast<int>(CommonType::ReqUseItem), 0x0A11);
    CHECK_EQ(static_cast<int>(CommonType::ToggleSafeAttackMode), 0x0A18);
    CHECK_EQ(static_cast<int>(CommonType::RequestActivateSpecAbility), 0x0A40);
    CHECK_EQ(static_cast<int>(CommonType::EquipItem), 0x0A02);

    // World: player at (2,2), monster at (5,4).
    StubTargetWorld w(20, 20, { 2, 2 });
    w.add({ 10, { 5, 4 }, TargetKind::Monster, true, false, false });

    // Context: action types + slot resolution.
    struct StubCtx : IWireContext {
        int16_t attackActionType(bool super) const override
        { return super ? 2 : 1; }
        int16_t consumableSlot(Input::UseItemSlot s) const override
        { return s == UseItemSlot::Hp ? 4 : 5; }
        bool shortcutBinding(uint8_t i, ShortcutBinding& out) const override
        {
            if (i >= 3) return false;
            out = slots[i];
            return out.isValid;
        }
        ShortcutBinding slots[3]{};
    };
    StubCtx ctx;
    ctx.slots[0] = ShortcutBinding{ true, false, 3, 0 };   // item in inventory slot 3
    ctx.slots[1] = ShortcutBinding{ true, true, 0, 12 };   // magic id 12
    ctx.slots[2] = ShortcutBinding{};                      // unbound

    // Move: locomotion -> Run / Move / Stop, wire dir ids, player position.
    auto mv = CommandTranslator::translate(
        PlayerCommand(PlayerMove{ Simulation::Direction::E, Locomotion::Running }), w, ctx);
    CHECK_EQ(mv.size(), 1u);
    CHECK(std::holds_alternative<Motion>(mv[0]));
    {
        const Motion& m = std::get<Motion>(mv[0]);
        CHECK_EQ(static_cast<int>(m.action), static_cast<int>(ActionType::Run));
        CHECK_EQ(static_cast<int>(m.dir), 3);           // E
        CHECK_EQ(m.x, 2); CHECK_EQ(m.y, 2);
    }

    auto mvv = CommandTranslator::translate(
        PlayerCommand(PlayerMove{ Simulation::Direction::NW, Locomotion::Walking }), w, ctx);
    {
        const Motion& m = std::get<Motion>(mvv[0]);
        CHECK_EQ(static_cast<int>(m.action), static_cast<int>(ActionType::Move));
        CHECK_EQ(static_cast<int>(m.dir), 8);           // NW
    }

    auto mvs = CommandTranslator::translate(
        PlayerCommand(PlayerMove{ Simulation::Direction::S, Locomotion::Standing }), w, ctx);
    {
        const Motion& m = std::get<Motion>(mvs[0]);
        CHECK_EQ(static_cast<int>(m.action), static_cast<int>(ActionType::Stop));
    }

    // Attack: facing toward target (SE=4), dest = target tile, normal action type.
    auto atk = CommandTranslator::translate(
        PlayerCommand(PlayerAttack{ 10u, false }), w, ctx);
    CHECK_EQ(atk.size(), 1u);
    CHECK(std::holds_alternative<MotionAttack>(atk[0]));
    {
        const MotionAttack& m = std::get<MotionAttack>(atk[0]);
        CHECK_EQ(static_cast<int>(m.action), static_cast<int>(ActionType::Attack));
        CHECK_EQ(static_cast<int>(m.dir), 4);           // SE toward (5,4)
        CHECK_EQ(m.x, 2); CHECK_EQ(m.y, 2);
        CHECK_EQ(m.destX, 5); CHECK_EQ(m.destY, 4);
        CHECK_EQ(m.weaponActionType, 1);                // normal
        CHECK_EQ(m.targetId, 10u);
    }

    // Attack with SUPER: super action type.
    auto atkS = CommandTranslator::translate(
        PlayerCommand(PlayerAttack{ 10u, true }), w, ctx);
    CHECK_EQ(std::get<MotionAttack>(atkS[0]).weaponActionType, 2);

    // Attack on a stale/unknown target: dropped, no packet.
    auto atkGone = CommandTranslator::translate(
        PlayerCommand(PlayerAttack{ 999u, false }), w, ctx);
    CHECK_EQ(atkGone.size(), 0u);

    // Cast: spell -> Motion Magic with dx = magic id; spec ability -> common.
    auto cast = CommandTranslator::translate(
        PlayerCommand(PlayerCast{ 5u, CastKind::Spell }), w, ctx);
    CHECK_EQ(cast.size(), 1u);
    {
        const Motion& m = std::get<Motion>(cast[0]);
        CHECK_EQ(static_cast<int>(m.action), static_cast<int>(ActionType::Magic));
        CHECK_EQ(m.dx, 5);
    }

    auto spec = CommandTranslator::translate(
        PlayerCommand(PlayerCast{ 0u, CastKind::SpecAbility }), w, ctx);
    CHECK(std::holds_alternative<Common>(spec[0]));
    CHECK_EQ(static_cast<int>(std::get<Common>(spec[0]).cmd),
             static_cast<int>(CommonType::RequestActivateSpecAbility));

    // UseItem: item-bound shortcut -> EquipItem with inventory slot.
    auto useSlot = CommandTranslator::translate(
        PlayerCommand(PlayerUseItem{ UseItemSlot::Shortcut1 }), w, ctx);
    CHECK(std::holds_alternative<Common>(useSlot[0]));
    CHECK_EQ(static_cast<int>(std::get<Common>(useSlot[0]).cmd),
             static_cast<int>(CommonType::EquipItem));
    CHECK_EQ(std::get<Common>(useSlot[0]).slot, 3);

    // UseItem: magic-bound shortcut -> Motion Magic with dx = magic id.
    auto useMagic = CommandTranslator::translate(
        PlayerCommand(PlayerUseItem{ UseItemSlot::Shortcut2 }), w, ctx);
    CHECK(std::holds_alternative<Motion>(useMagic[0]));
    CHECK_EQ(std::get<Motion>(useMagic[0]).dx, 12);

    // UseItem: unbound shortcut -> dropped.
    auto useNone = CommandTranslator::translate(
        PlayerCommand(PlayerUseItem{ UseItemSlot::Shortcut3 }), w, ctx);
    CHECK_EQ(useNone.size(), 0u);

    // UseItem: HP/MP potions -> ReqUseItem with the resolved slot.
    auto useHp = CommandTranslator::translate(
        PlayerCommand(PlayerUseItem{ UseItemSlot::Hp }), w, ctx);
    {
        const Common& c = std::get<Common>(useHp[0]);
        CHECK_EQ(static_cast<int>(c.cmd), static_cast<int>(CommonType::ReqUseItem));
        CHECK_EQ(c.slot, 4);
    }
    auto useMp = CommandTranslator::translate(
        PlayerCommand(PlayerUseItem{ UseItemSlot::Mp }), w, ctx);
    CHECK_EQ(std::get<Common>(useMp[0]).slot, 5);

    // Toggles: stance + safe-attack emit; run/force-attack/window are local.
    auto stance = CommandTranslator::translate(
        PlayerCommand(PlayerToggle{ ToggleKind::Stance, true, 0 }), w, ctx);
    CHECK(std::holds_alternative<Common>(stance[0]));
    CHECK_EQ(static_cast<int>(std::get<Common>(stance[0]).cmd),
             static_cast<int>(CommonType::ToggleCombatMode));

    auto safe = CommandTranslator::translate(
        PlayerCommand(PlayerToggle{ ToggleKind::SafeAttack, true, 0 }), w, ctx);
    CHECK_EQ(static_cast<int>(std::get<Common>(safe[0]).cmd),
             static_cast<int>(CommonType::ToggleSafeAttackMode));

    auto run = CommandTranslator::translate(
        PlayerCommand(PlayerToggle{ ToggleKind::Run, true, 0 }), w, ctx);
    CHECK_EQ(run.size(), 0u);

    auto force = CommandTranslator::translate(
        PlayerCommand(PlayerToggle{ ToggleKind::ForceAttack, true, 0 }), w, ctx);
    CHECK_EQ(force.size(), 0u);

    auto win = CommandTranslator::translate(
        PlayerCommand(PlayerToggle{ ToggleKind::Window, true, 2 }), w, ctx);
    CHECK_EQ(win.size(), 0u);

    // SetTarget never reaches the wire directly (it drives navigation).
    auto target = CommandTranslator::translate(
        PlayerCommand(PlayerSetTarget{ { 4, 4 }, 0, false, TargetVerb::Attack }), w, ctx);
    CHECK_EQ(target.size(), 0u);
}

void testHelbreathPacketEncoder()
{
    using Protocol::ActionType;
    using Protocol::Common;
    using Protocol::CommonType;
    using Protocol::EncodeContext;
    using Protocol::HelbreathPacketEncoder;
    using Protocol::Motion;
    using Protocol::MotionAttack;
    using Protocol::ProtocolCommand;

    // Wire msg ids (hb_lite MsgId).
    CHECK_EQ(Protocol::kMsgIdMotion, 0x0FA314D5u);
    CHECK_EQ(Protocol::kMsgIdCommon, 0x0FA314DCu);

    auto expectBytes = [](const std::vector<uint8_t>& got,
                          const std::vector<uint8_t>& want) {
        if (got == want) return;
        ++g_failures;
        std::printf("FAIL byte-serialization: got %zu bytes, want %zu\n",
                    got.size(), want.size());
        if (got.size() != want.size()) return;
        for (size_t i = 0; i < got.size(); ++i)
            std::printf("  [%zu] got=%02X want=%02X\n", i, got[i], want[i]);
    };

    EncodeContext ctx;
    ctx.timeMs = 0x11223344u;

    // Motion Move/E, player (2,2): PacketCommandMotionSimple (21 bytes).
    auto mv = HelbreathPacketEncoder::encode(
        ProtocolCommand(Motion{ ActionType::Move, 3, 2, 2, 0, 0, 0 }), ctx);
    CHECK_EQ(mv.size(), 21u);
    CHECK_EQ(HelbreathPacketEncoder::encodedSize(
                 ProtocolCommand(Motion{ ActionType::Move, 3, 2, 2, 0, 0, 0 })),
             21u);
    expectBytes(mv, {
        0xD5, 0x14, 0xA3, 0x0F, // msg_id CommandMotion
        0x01, 0x00,             // Move
        0x02, 0x00,             // x = 2
        0x02, 0x00,             // y = 2
        0x03,                   // dir = E
        0x00, 0x00,             // dx
        0x00, 0x00,             // dy
        0x00, 0x00,             // type
        0x44, 0x33, 0x22, 0x11  // time_ms
    });

    // MotionAttack Attack/SE toward (5,4), player (2,2), normal type, target 10
    // -> PacketCommandMotionAttack (23 bytes).
    auto atk = HelbreathPacketEncoder::encode(
        ProtocolCommand(MotionAttack{ ActionType::Attack, 4, 2, 2, 5, 4, 1, 10 }), ctx);
    CHECK_EQ(atk.size(), 23u);
    expectBytes(atk, {
        0xD5, 0x14, 0xA3, 0x0F, // msg_id CommandMotion
        0x03, 0x00,             // Attack
        0x02, 0x00,             // x = 2
        0x02, 0x00,             // y = 2
        0x04,                   // dir = SE
        0x05, 0x00,             // destX = 5
        0x04, 0x00,             // destY = 4
        0x01, 0x00,             // weaponActionType = 1
        0x0A, 0x00,             // target_id = 10
        0x44, 0x33, 0x22, 0x11  // time_ms
    });

    // Common ToggleCombatMode, player (2,2), no slot -> PacketCommandCommonWithTime
    // (27 bytes), v1 = 0 (absent slot).
    EncodeContext ctx2;
    ctx2.timeMs = 0xCAFEBABEu;
    auto com = HelbreathPacketEncoder::encode(
        ProtocolCommand(Common{ CommonType::ToggleCombatMode, 2, 2, 0, -1, 0 }), ctx2);
    CHECK_EQ(com.size(), 27u);
    expectBytes(com, {
        0xDC, 0x14, 0xA3, 0x0F, // msg_id CommandCommon
        0x0B, 0x0A,             // msg_type ToggleCombatMode
        0x02, 0x00,             // x = 2
        0x02, 0x00,             // y = 2
        0x00,                   // dir
        0x00, 0x00, 0x00, 0x00, // v1 = 0 (no slot)
        0x00, 0x00, 0x00, 0x00, // v2
        0x00, 0x00, 0x00, 0x00, // v3
        0xBE, 0xBA, 0xFE, 0xCA  // time_ms
    });

    // Common ReqUseItem, slot 4 -> v1 = 4, plus idx feeds v2.
    auto item = HelbreathPacketEncoder::encode(
        ProtocolCommand(Common{ CommonType::ReqUseItem, 2, 2, 0, 4, 0 }), ctx2);
    CHECK_EQ(item.size(), 27u);
    expectBytes(item, {
        0xDC, 0x14, 0xA3, 0x0F,
        0x11, 0x0A,             // ReqUseItem
        0x02, 0x00, 0x02, 0x00, 0x00,
        0x04, 0x00, 0x00, 0x00, // v1 = slot 4
        0x00, 0x00, 0x00, 0x00, // v2
        0x00, 0x00, 0x00, 0x00, // v3
        0xBE, 0xBA, 0xFE, 0xCA
    });

    // Determinism: same command + same ctx -> identical bytes.
    const auto again = HelbreathPacketEncoder::encode(
        ProtocolCommand(MotionAttack{ ActionType::Attack, 4, 2, 2, 5, 4, 1, 10 }), ctx);
    CHECK(again == atk);
}

void testSingleStepCadence()
{
    GridWorld w = makeOpenGrid(10, 10);
    PlayerMovementSimulation sim;
    sim.setWorld(&w);
    sim.setTilePosition(5, 5);

    // Idle: opportunity to arm a step.
    CHECK(sim.beginStepOpportunity());
    sim.update(16.0f);
    CHECK(sim.beginStepOpportunity());

    // Arm a single step E. It commits on the next update but must NOT
    // auto-chain into further steps: the tail waits for the navigator.
    sim.beginSingleStep(Direction::E, Locomotion::Walking);
    CHECK(!sim.beginStepOpportunity());          // intent armed
    sim.update(100.0f);                          // commits, mid-flight
    CHECK(sim.activeStep().active);
    CHECK(!sim.beginStepOpportunity());          // still mid-flight
    sim.update(WALK_DURATION_MS + 1.0f);         // lands at (6,5)
    CHECK_EQ(sim.tilePosition().x, 6);
    CHECK_EQ(sim.tilePosition().y, 5);
    CHECK(sim.beginStepOpportunity());           // tail may re-arm

    // Nothing armed -> the sim must not move on its own.
    sim.update(WALK_DURATION_MS + 1.0f);
    sim.update(WALK_DURATION_MS + 1.0f);
    CHECK_EQ(sim.tilePosition().x, 6);

    // Per-step turn cadence: arm S now (path change mid-journey). Arm at idle
    // commits on the next update and lands on the one after.
    sim.beginSingleStep(Direction::S, Locomotion::Running);
    sim.update(100.0f);                          // commit, mid-flight
    CHECK_EQ(sim.tilePosition().x, 6);
    CHECK_EQ(sim.tilePosition().y, 5);
    CHECK_EQ(sim.activeStep().durationMs, RUN_DURATION_MS);
    CHECK_EQ(sim.activeStep().direction, Direction::S);
    CHECK(!sim.beginStepOpportunity());
    sim.update(RUN_DURATION_MS + 1.0f);          // lands at (6,6)
    CHECK_EQ(sim.tilePosition().y, 6);
    CHECK(sim.beginStepOpportunity());

    // Blocked single step is dropped, leaving an opportunity to re-arm.
    GridWorld wb = makeOpenGrid(10, 10);
    wb.setWalkable(6, 6, false);                 // wall east of start
    PlayerMovementSimulation sb;
    sb.setWorld(&wb);
    sb.setTilePosition(5, 6);
    sb.beginSingleStep(Direction::E, Locomotion::Walking);
    sb.update(16.0f);                            // blocked: no commit
    CHECK(!sb.activeStep().active);
    CHECK_EQ(sb.tilePosition().x, 5);
    CHECK(sb.beginStepOpportunity());            // arm dropped -> re-arm ready

    // Regression guard: persistent joystick input still auto-chains.
    GridWorld wj = makeOpenGrid(10, 10);
    PlayerMovementSimulation sj;
    sj.setWorld(&wj);
    sj.setTilePosition(0, 0);
    sj.handleInput(Direction::E, Locomotion::Walking);
    for (int i = 0; i < 6; ++i)                  // first update commits
        sj.update(WALK_DURATION_MS + 1.0f);
    CHECK_EQ(sj.tilePosition().x, 5);            // 5 committed steps
}

} // namespace

// ── MobileControlsHud (item 7: multitouch producer) ─────────────────────
// Logical canvas = 1280x720. Tests use identity logical coords and a
// right-band screenToTile that maps x>=896 onto tile (x-896)/32 (32px tiles).

namespace hudtest {

struct Builder {
    using P = Input::TouchPoint;

    Input::TouchFrame press(float x, float y, int id = 0) const
    {
        Input::TouchFrame f;
        f.points[f.count++] = P{ id, x, y, true, true, false };
        return f;
    }
    Input::TouchFrame hold(float x, float y, int id = 0) const
    {
        Input::TouchFrame f;
        f.points[f.count++] = P{ id, x, y, false, true, false };
        return f;
    }
    Input::TouchFrame release(float x, float y, int id = 0) const
    {
        Input::TouchFrame f;
        f.points[f.count++] = P{ id, x, y, false, false, true };
        return f;
    }
    Input::TouchFrame multi(const P& a, const P& b) const
    {
        Input::TouchFrame f;
        f.points[f.count++] = a;
        f.points[f.count++] = b;
        return f;
    }
    Input::TouchFrame multi(const Input::TouchFrame& a, const P& b) const
    {
        Input::TouchFrame f = a;
        f.points[f.count++] = b;
        return f;
    }
    Input::TouchFrame multi(const P& a, const Input::TouchFrame& b) const
    {
        Input::TouchFrame f;
        f.points[f.count++] = a;
        for (uint16_t i = 0; i < b.count; ++i)
            f.points[f.count++] = b.points[i];
        return f;
    }
    Input::TouchFrame multi(const Input::TouchFrame& a, const Input::TouchFrame& b) const
    {
        Input::TouchFrame f = a;
        for (uint16_t i = 0; i < b.count; ++i)
            f.points[f.count++] = b.points[i];
        return f;
    }
};

inline Simulation::GridCoord stt(float x, float y)
{
    return Simulation::GridCoord{ static_cast<int>((x - 896.0f) / 32.0f),
                                  static_cast<int>(std::floor(y / 32.0f)) };
}

template <class T>
const T* hudFind(const Input::PlayerInputFrame& f)
{
    for (auto it = f.begin(); it != f.end(); ++it)
        if (const T* p = std::get_if<T>(it)) return p;
    return nullptr;
}

void run()
{
    using namespace Input;
    using namespace HUD;

    MobileControlsHud hud;
    const Builder tap;

    StubTargetWorld world(40, 16, GridCoord{ 5, 5 });
    world.setPlayerId(1);
    world.add(TargetInfo{ 2, { 0, 5 }, Simulation::TargetKind::Monster,
                          true, false, false });
    world.add(TargetInfo{ 3, { 2, 5 }, Simulation::TargetKind::Item,
                          false, true, false });
    world.add(TargetInfo{ 4, { 4, 5 }, Simulation::TargetKind::Npc,
                          true, false, true });
    world.add(TargetInfo{ 1, { 5, 5 }, Simulation::TargetKind::Monster,
                          true, false, false }); // player's own tile

    // ── ground tap → SetTarget{Move} ────────────────────────────────────
    {
        Input::PlayerInputFrame f = hud.update(tap.press(1160.0f, 232.0f),
                                               KeyState{}, world, stt, 0.016f);
        f = hud.update(tap.release(1160.0f, 232.0f), KeyState{}, world, stt,
                       0.016f);
        CHECK_EQ(f.count(), 1u);
        const PlayerSetTarget* st = hudFind<PlayerSetTarget>(f);
        CHECK(st && st->verb == TargetVerb::Move && !st->hasTargetId);
        CHECK_EQ(st->position.x, 8);
        CHECK_EQ(st->position.y, 7);
        CHECK(!hud.target().valid); // ground tap never becomes an ATK target
    }

    // ── enemy tap → SetTarget{Attack, id} + ATK button target ───────────
    {
        Input::PlayerInputFrame f = hud.update(tap.press(904.0f, 168.0f),
                                               KeyState{}, world, stt, 0.016f);
        f = hud.update(tap.release(904.0f, 168.0f), KeyState{}, world, stt,
                       0.016f);
        CHECK_EQ(f.count(), 1u);
        const PlayerSetTarget* st = hudFind<PlayerSetTarget>(f);
        CHECK(st && st->hasTargetId && st->verb == TargetVerb::Attack);
        CHECK_EQ(st->targetId, 2u);
        CHECK(hud.target().valid && hud.target().id == 2u);
        CHECK(hud.view().attackEnabled);

        // ATK press fires immediately against the engaged target.
        f = hud.update(tap.press(1134.0f, 568.0f), KeyState{}, world, stt,
                       0.016f);
        const PlayerAttack* atk = hudFind<PlayerAttack>(f);
        CHECK(atk && atk->targetId == 2u && !atk->super);
        CHECK(!hud.view().joystickActive);
        CHECK(!hud.view().reticleActive);

        // one tap = one attack (no repeat without holding)
        f = hud.update(tap.release(1134.0f, 568.0f), KeyState{}, world, stt,
                       0.016f);
        CHECK_EQ(f.count(), 0u);
    }

    // ── untargeted ATK press → PlayerAttack{ targetId = 0 } ─────────────
    {
        MobileControlsHud freshHud;
        CHECK(!freshHud.target().valid);
        Input::PlayerInputFrame fAtk = freshHud.update(tap.press(1134.0f, 568.0f),
                                                      KeyState{}, world, stt, 0.016f);
        CHECK(freshHud.view().attackEnabled);
        const PlayerAttack* atk = hudFind<PlayerAttack>(fAtk);
        CHECK(atk && atk->targetId == 0u && !atk->super);
        freshHud.update(tap.release(1134.0f, 568.0f), KeyState{}, world, stt, 0.016f);
    }

    // ── item tap → SetTarget{Pickup} ────────────────────────────────────
    {
        Input::PlayerInputFrame f = hud.update(tap.press(968.0f, 168.0f),
                                               KeyState{}, world, stt, 0.016f);
        f = hud.update(tap.release(968.0f, 168.0f), KeyState{}, world, stt,
                       0.016f);
        const PlayerSetTarget* st = hudFind<PlayerSetTarget>(f);
        CHECK(st && st->hasTargetId && st->verb == TargetVerb::Pickup);
        CHECK_EQ(st->targetId, 3u);
    }

    // ── NPC tap → ring; ring verb picks and fires ───────────────────────
    {
        Input::PlayerInputFrame f = hud.update(tap.press(1032.0f, 168.0f),
                                               KeyState{}, world, stt, 0.016f);
        f = hud.update(tap.release(1032.0f, 168.0f), KeyState{}, world, stt,
                       0.016f);
        CHECK_EQ(f.count(), 0u); // ring defers the decision
        CHECK(hud.view().ringOpen);
        CHECK_EQ(hud.ringTile().x, 4);
        CHECK_EQ(hud.ringTile().y, 5);

        // E ring button = Attack (button actions emit on the press frame)
        Input::PlayerInputFrame fAtk = hud.update(tap.press(1116.0f, 168.0f),
                                                  KeyState{}, world, stt, 0.016f);
        hud.update(tap.release(1116.0f, 168.0f), KeyState{}, world, stt,
                   0.016f);
        const PlayerSetTarget* st = hudFind<PlayerSetTarget>(fAtk);
        CHECK(st && st->hasTargetId && st->verb == TargetVerb::Attack);
        CHECK_EQ(st->targetId, 4u);
        CHECK(!hud.view().ringOpen);

        // S ring button = Interact (trade)
        Input::PlayerInputFrame fNpc = hud.update(tap.press(1032.0f, 168.0f),
                                                  KeyState{}, world, stt, 0.016f);
        hud.update(tap.release(1032.0f, 168.0f), KeyState{}, world, stt,
                   0.016f);
        Input::PlayerInputFrame fTrd = hud.update(tap.press(1032.0f, 252.0f),
                                                  KeyState{}, world, stt, 0.016f);
        hud.update(tap.release(1032.0f, 252.0f), KeyState{}, world, stt,
                   0.016f);
        st = hudFind<PlayerSetTarget>(fTrd);
        CHECK(st && st->verb == TargetVerb::Interact);
        CHECK_EQ(st->targetId, 4u);

        // outside-tap on the ring closes it
        f = hud.update(tap.press(1032.0f, 168.0f), KeyState{}, world, stt,
                       0.016f);
        f = hud.update(tap.release(1032.0f, 168.0f), KeyState{}, world, stt,
                       0.016f);
        CHECK(hud.view().ringOpen);
        f = hud.update(tap.press(400.0f, 400.0f), KeyState{}, world, stt,
                       0.016f);
        f = hud.update(tap.release(400.0f, 400.0f), KeyState{}, world, stt,
                       0.016f);
        CHECK(!hud.view().ringOpen);
        CHECK_EQ(f.count(), 0u);
    }

    // ── self-tile tap is a strict no-op ─────────────────────────────────
    {
        Input::PlayerInputFrame f = hud.update(tap.press(1064.0f, 168.0f),
                                               KeyState{}, world, stt, 0.016f);
        CHECK(!hud.view().reticleValid);
        f = hud.update(tap.release(1064.0f, 168.0f), KeyState{}, world, stt,
                       0.016f);
        CHECK_EQ(f.count(), 0u);
    }

    // ── joystick: press → drag → release; below dead zone stays idle ────
    {
        Input::PlayerInputFrame f = hud.update(tap.press(300.0f, 300.0f),
                                               KeyState{}, world, stt, 0.016f);
        CHECK_EQ(f.count(), 0u);
        f = hud.update(tap.hold(306.0f, 300.0f), KeyState{}, world, stt,
                       0.016f); // ~6px < 0.22*56 = 12.3px dead zone
        CHECK_EQ(f.count(), 0u);

        f = hud.update(tap.hold(400.0f, 300.0f), KeyState{}, world, stt,
                       0.016f); // 100px E
        CHECK_EQ(f.count(), 1u);
        const PlayerMove* mv = hudFind<PlayerMove>(f);
        CHECK(mv && mv->direction == Simulation::Direction::E);
        CHECK(mv->locomotion == Simulation::Locomotion::Walking);
        CHECK(hud.view().joystickActive);

        f = hud.update(tap.release(400.0f, 300.0f), KeyState{}, world, stt,
                       0.016f);
        CHECK_EQ(f.count(), 0u);
        CHECK(!hud.view().joystickActive);
    }

    // ── a touch on a button is never a gesture (Rule 1) ─────────────────
    // ── a touch on a button is never a gesture (Rule 1) ─────────────────
    {
        Input::PlayerInputFrame fPress = hud.update(tap.press(860.0f, 640.0f),
                                                    KeyState{}, world, stt, 0.016f);
        Input::PlayerInputFrame fRel = hud.update(tap.release(860.0f, 640.0f),
                                                  KeyState{}, world, stt, 0.016f);
        const PlayerUseItem* u = hudFind<PlayerUseItem>(fPress);
        CHECK(u && u->slot == UseItemSlot::Hp);
        CHECK_EQ(fPress.count(), 1u);
        CHECK_EQ(fRel.count(), 0u);
        CHECK(!hud.view().joystickActive && !hud.view().reticleActive);
    }

    // ── SUPER modifier: held SUPER + ATK press = super attack ───────────
    {
        // re-engage an enemy first
        Input::PlayerInputFrame f = hud.update(tap.press(904.0f, 168.0f),
                                               KeyState{}, world, stt, 0.016f);
        f = hud.update(tap.release(904.0f, 168.0f), KeyState{}, world, stt,
                       0.016f);

        const Builder::P super{ 1, 1130.0f, 500.0f, true, true, false };
        const Builder::P atk{ 2, 1134.0f, 568.0f, true, true, false };
        f = hud.update(tap.multi(super, atk), KeyState{}, world, stt, 0.016f);
        const PlayerAttack* a = hudFind<PlayerAttack>(f);
        CHECK(a && a->targetId == 2u && a->super);
        CHECK(hud.view().superHeld);
    }

    // ── RUN button tap: toggles run persistent mode ON/OFF ─────────────
    {
        // Tap RUN button -> toggle persistent run ON
        Input::PlayerInputFrame fOn = hud.update(tap.press(1020.0f, 500.0f),
                                                 KeyState{}, world, stt, 0.016f);
        hud.update(tap.release(1020.0f, 500.0f), KeyState{}, world, stt, 0.016f);
        const PlayerToggle* togg = hudFind<PlayerToggle>(fOn);
        CHECK(togg && togg->kind == ToggleKind::Run && togg->on);
        CHECK(hud.view().runHeld);

        // Movement with joystick while RUN ON -> Running
        hud.update(tap.press(300.0f, 300.0f), KeyState{}, world, stt, 0.016f);
        Input::PlayerInputFrame fJoy = hud.update(tap.hold(400.0f, 300.0f),
                                                  KeyState{}, world, stt, 0.016f);
        const PlayerMove* mv = hudFind<PlayerMove>(fJoy);
        CHECK(mv && mv->locomotion == Simulation::Locomotion::Running);
        hud.update(tap.release(400.0f, 300.0f), KeyState{}, world, stt, 0.016f);

        // Tap RUN button again -> toggle persistent run OFF
        Input::PlayerInputFrame fOff = hud.update(tap.press(1020.0f, 500.0f),
                                                  KeyState{}, world, stt, 0.016f);
        hud.update(tap.release(1020.0f, 500.0f), KeyState{}, world, stt, 0.016f);
        togg = hudFind<PlayerToggle>(fOff);
        CHECK(togg && togg->kind == ToggleKind::Run && !togg->on);
        CHECK(!hud.view().runHeld);
    }

    // ── stance toggle via button ────────────────────────────────────────
    {
        Input::PlayerInputFrame fOn = hud.update(tap.press(1240.0f, 560.0f),
                                                 KeyState{}, world, stt, 0.016f);
        hud.update(tap.release(1240.0f, 560.0f), KeyState{}, world, stt, 0.016f);
        const PlayerToggle* t = hudFind<PlayerToggle>(fOn);
        CHECK(t && t->kind == ToggleKind::Stance && t->on);
        CHECK(hud.view().stanceOn);
        Input::PlayerInputFrame fOff = hud.update(tap.press(1240.0f, 560.0f),
                                                  KeyState{}, world, stt, 0.016f);
        hud.update(tap.release(1240.0f, 560.0f), KeyState{}, world, stt, 0.016f);
        t = hudFind<PlayerToggle>(fOff);
        CHECK(t && t->kind == ToggleKind::Stance && !t->on);
        CHECK(!hud.view().stanceOn);
    }

    // ── ☰ menu: open, window pick, outside-tap collapse ─────────────────
    {
        Input::PlayerInputFrame f = hud.update(tap.press(1220.0f, 34.0f),
                                               KeyState{}, world, stt, 0.016f);
        CHECK(hud.view().menuOpen);
        f = hud.update(tap.release(1220.0f, 34.0f), KeyState{}, world, stt,
                       0.016f);
        CHECK_EQ(f.count(), 0u);

        // window2 (Magics)
        Input::PlayerInputFrame fW = hud.update(tap.press(1100.0f, 150.0f),
                                                KeyState{}, world, stt, 0.016f);
        hud.update(tap.release(1100.0f, 150.0f), KeyState{}, world, stt,
                   0.016f);
        const PlayerToggle* t = hudFind<PlayerToggle>(fW);
        CHECK(t && t->kind == ToggleKind::Window && t->on);
        CHECK_EQ(t->windowId, 2u);
        CHECK(!hud.view().menuOpen);

        f = hud.update(tap.press(1220.0f, 34.0f), KeyState{}, world, stt,
                       0.016f);
        CHECK(hud.view().menuOpen);
        f = hud.update(tap.press(400.0f, 400.0f), KeyState{}, world, stt,
                       0.016f); // outside-tap collapses + consumes
        CHECK(!hud.view().menuOpen);
        CHECK_EQ(f.count(), 0u);
    }

    // ── simultaneous joystick + reticle multitouch ──────────────────────
    {
        const Builder::P joy{ 0, 400.0f, 300.0f, false, true, false };
        const Builder::P ret{ 1, 904.0f, 168.0f, false, true, false };
        Input::PlayerInputFrame f = hud.update(
            tap.multi(tap.press(300.0f, 300.0f, 0),
                      tap.press(904.0f, 168.0f, 1)),
            KeyState{}, world, stt, 0.016f);
        CHECK_EQ(f.count(), 0u);
        CHECK(hud.view().joystickActive);
        CHECK(hud.view().reticleActive);

        f = hud.update(tap.multi(joy, ret), KeyState{}, world, stt, 0.016f);
        const PlayerMove* mv = hudFind<PlayerMove>(f);
        CHECK(mv && mv->direction == Simulation::Direction::E);
        CHECK(!hudFind<PlayerSetTarget>(f)); // reticle fires only on release

        f = hud.update(tap.multi(tap.release(400.0f, 300.0f, 0),
                                 tap.release(904.0f, 168.0f, 1)),
                       KeyState{}, world, stt, 0.016f);
        const PlayerSetTarget* st = hudFind<PlayerSetTarget>(f);
        CHECK(st && st->hasTargetId && st->verb == TargetVerb::Attack);
        CHECK_EQ(st->targetId, 2u);
    }

    // ── keyboard parity (desktop) ───────────────────────────────────────
    {
        MobileControlsHud h;
        const Input::TouchFrame noTouch{}; // no joystick/touch so keys route
        KeyState k{};
        k.moveRight = true;
        Input::PlayerInputFrame f = h.update(noTouch, k, world, stt, 0.016f);
        const PlayerMove* mv = hudFind<PlayerMove>(f);
        CHECK(mv && mv->direction == Simulation::Direction::E);
        CHECK(mv->locomotion == Simulation::Locomotion::Walking);

        KeyState kshift = k;
        kshift.shiftHeld = true;
        f = h.update(noTouch, kshift, world, stt, 0.016f);
        mv = hudFind<PlayerMove>(f);
        CHECK(mv && mv->locomotion == Simulation::Locomotion::Running);

        // Ctrl+R persists the run mode
        KeyState krun = k;
        krun.ctrlRRun = true;
        f = h.update(noTouch, krun, world, stt, 0.016f);
        const PlayerToggle* t = hudFind<PlayerToggle>(f);
        CHECK(t && t->kind == ToggleKind::Run && t->on);
        f = h.update(noTouch, k, world, stt, 0.016f);
        mv = hudFind<PlayerMove>(f);
        CHECK(mv && mv->locomotion == Simulation::Locomotion::Running);

        // one-shot keys
        KeyState k2{};
        k2.tabStance  = true;
        k2.homeSafe   = true;
        k2.ctrlAForce = true;
        k2.pageUp     = true;
        k2.f2         = true;
        k2.f5         = true;
        f = h.update(noTouch, k2, world, stt, 0.016f);
        CHECK(hudFind<PlayerToggle>(f) && hudFind<PlayerCast>(f));
        const PlayerUseItem* u = hudFind<PlayerUseItem>(f);
        CHECK(u && u->slot == UseItemSlot::Shortcut1);

        // Insert hold-repeat (potions at 500ms)
        KeyState k3{};
        k3.hpInsertHeld = true;
        Input::PlayerInputFrame fh = h.update(noTouch, k3, world, stt, 0.016f);
        CHECK(hudFind<PlayerUseItem>(fh)
               && hudFind<PlayerUseItem>(fh)->slot == UseItemSlot::Hp);
        fh = h.update(noTouch, k3, world, stt, 0.6f);
        CHECK(hudFind<PlayerUseItem>(fh) != nullptr); // repeat fired once
    }

    // ── potion button repeat cadence ────────────────────────────────────
    {
        MobileControlsHud h;
        Input::PlayerInputFrame f = h.update(tap.press(860.0f, 640.0f),
                                             KeyState{}, world, stt, 0.016f);
        f = h.update(tap.hold(860.0f, 640.0f), KeyState{}, world, stt, 0.6f);
        int hp = 0;
        for (auto it = f.begin(); it != f.end(); ++it)
            if (const PlayerUseItem* u = std::get_if<PlayerUseItem>(it))
                if (u->slot == UseItemSlot::Hp) ++hp;
        CHECK_EQ(hp, 1); // press + one 500ms repeat, not a runaway
    }
}

} // namespace hudtest

int main()
{
    testDirectionOffsets();
    testDirectionQuantizer();
    testDeadZone();
    testWalkRunDurations();
    testStepCannotBeRedirected();
    testIntentsCommitOrder();
    testRejections();
    testSingleStepCadence();
    testInterpolation();
    testGridWorld();
    testJson();
    testDirectionName();
    testAtlasV2();
    testTileMap();
    testSplitPoints();
    testPlayerCommandBoundary();
    testTargetWorldStub();
    testTargetResolver();
    testGreedyNavigator();
    testNavExecutor();
    testCommandTranslator();
    testHelbreathPacketEncoder();
    hudtest::run();

    std::printf("gridplay_tests: %d checks, %d failures\n",
                g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}