// Focused unit tests for the P0a simulation/input logic.
// Header-light: no raylib graphics calls, so it can run as a desktop-only
// console test executable. Invoked by CMakeLists.txt as `gridplay_tests`.

#include <cmath>
#include <cstdio>

#include "Game/Simulation/GridWorld.h"
#include "Game/Simulation/PlayerMovementSimulation.h"
#include "Game/Input/DirectionQuantizer.h"
#include "Game/Input/JoystickInput.h"

using namespace Simulation;

namespace {

int g_checks   = 0;
int g_failures = 0;

#define CHECK(cond)                                                      \
    do {                                                                 \
        ++g_checks;                                                      \
        if (!(cond)) {                                                   \
            ++g_failures;                                                \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);  \
        }                                                                \
    } while (0)

#define CHECK_EQ(a, b)                                                   \
    do {                                                                 \
        ++g_checks;                                                      \
        auto va = (a);                                                   \
        auto vb = (b);                                                   \
        if (!(va == vb)) {                                               \
            ++g_failures;                                                \
            std::printf("FAIL %s:%d  %s == %s (%d vs %d)\n",             \
                        __FILE__, __LINE__, #a, #b, (int)va, (int)vb);   \
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
    CHECK(p0.pixelX == 5 * 32.0f);
    CHECK(p0.pixelY == 5 * 32.0f);

    sim.handleInput(Direction::E, Locomotion::Walking);
    sim.update(1000.0f);                     // commit E step
    sim.update(280.0f);                      // half step (560/2)
    auto p1 = sim.presentation(32.0f);
    CHECK(p1.isMoving);
    CHECK(p1.stepProgress > 0.0f);
    CHECK(p1.pixelX > 5 * 32.0f && p1.pixelX < 6 * 32.0f);

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

    std::printf("gridplay_tests: %d checks, %d failures\n",
                g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}