// Focused unit tests for the P0a simulation/input logic.
// Header-light: no raylib graphics calls, so it can run as a desktop-only
// console test executable. Invoked by CMakeLists.txt as `gridplay_tests`.

#include <cmath>
#include <cstdio>
#include <string>
#include <type_traits>

#include "Game/Simulation/GridWorld.h"
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

} // namespace

int main()
{
    testDirectionOffsets();
    testDirectionQuantizer();
    testDeadZone();
    testWalkRunDurations();
    testStepCannotBeRedirected();
    testIntentsCommitOrder();
    testRejections();
    testInterpolation();
    testGridWorld();
    testJson();
    testDirectionName();
    testAtlasV2();
    testTileMap();
    testSplitPoints();

    std::printf("gridplay_tests: %d checks, %d failures\n",
                g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}