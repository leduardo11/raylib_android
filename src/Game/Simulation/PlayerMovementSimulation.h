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
};

} // namespace Simulation
