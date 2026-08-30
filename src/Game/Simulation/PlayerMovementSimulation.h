#pragma once

#include "GridCoord.h"
#include "HelbreathDirection.h"
#include "MovementTiming.h"
#include "MoveIntent.h"
#include "ActiveStep.h"
#include "GridWorld.h"

namespace Simulation {

class PlayerMovementSimulation {
public:
    PlayerMovementSimulation();

    void setWorld(GridWorld* world);
    void setTilePosition(int x, int y);
    void setTilePosition(const GridCoord& pos);

    void update(float dtMs);
    void handleInput(Direction dir, Locomotion loco);
    void releaseInput();

    // Single-step drive seam for the NavExecutor: arm exactly one step in
    // `dir`. The intent is consumed at commit and never auto-chains, so the
    // caller (Game loop) re-arms per committed step:
    //
    //   while (nav engaged && sim.beginStepOpportunity())
    //       -> nav.nextMove -> sim.beginSingleStep(dir, loco)
    void beginSingleStep(Direction dir, Locomotion loco);

    // True when a step may be armed: no step in progress and no motion intent
    // already pending. The caller evaluates the navigator only then.
    bool beginStepOpportunity() const
    {
        return !m_activeStep.active && !m_pendingIntent.active;
    }

    GridCoord tilePosition() const { return m_tilePosition; }
    GridCoord destinationPosition() const;
    const ActiveStep& activeStep() const { return m_activeStep; }
    const MoveIntent& pendingIntent() const { return m_pendingIntent; }

    struct PresentationData {
        float pixelX;
        float pixelY;
        Direction facing;
        Locomotion locomotion;
        float stepProgress;
        bool  isMoving;
    };

    PresentationData presentation(float tileSize) const;

    static constexpr float smoothStep(float t)
    {
        if (t <= 0.0f) return 0.0f;
        if (t >= 1.0f) return 1.0f;
        return t * t * (3.0f - 2.0f * t);
    }

private:
    void commitStep(const MoveIntent& intent);
    void resolveNextIntent();

    GridWorld* m_world = nullptr;
    GridCoord  m_tilePosition;
    ActiveStep m_activeStep;
    MoveIntent m_pendingIntent;
    bool m_singleStepPending = false; // armed by beginSingleStep, not yet consumed
};

} // namespace Simulation
