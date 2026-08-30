#include "PlayerMovementSimulation.h"

namespace Simulation {

PlayerMovementSimulation::PlayerMovementSimulation()
    : m_tilePosition{15, 10}
{
}

void PlayerMovementSimulation::setWorld(GridWorld* world)
{
    m_world = world;
}

void PlayerMovementSimulation::setTilePosition(int x, int y)
{
    m_tilePosition = {x, y};
}

void PlayerMovementSimulation::setTilePosition(const GridCoord& pos)
{
    m_tilePosition = pos;
}

void PlayerMovementSimulation::update(float dtMs)
{
    if (m_activeStep.active)
    {
        m_activeStep.elapsedMs += dtMs;
        if (m_activeStep.isComplete())
        {
            m_tilePosition = m_activeStep.destination;
            m_activeStep.active = false;
            resolveNextIntent();
        }
    }
    else
    {
        resolveNextIntent();
    }
}

void PlayerMovementSimulation::handleInput(Direction dir, Locomotion loco)
{
    m_pendingIntent.direction  = dir;
    m_pendingIntent.locomotion = loco;
    m_pendingIntent.active     = true;
}

void PlayerMovementSimulation::releaseInput()
{
    m_pendingIntent.active = false;
}

GridCoord PlayerMovementSimulation::destinationPosition() const
{
    if (m_activeStep.active)
        return m_activeStep.destination;

    if (m_pendingIntent.active)
    {
        auto off = directionOffset(m_pendingIntent.direction);
        return m_tilePosition + off;
    }

    return m_tilePosition;
}

void PlayerMovementSimulation::commitStep(const MoveIntent& intent)
{
    auto off = directionOffset(intent.direction);
    GridCoord dest = m_tilePosition + off;

    if (!m_world || !m_world->canStepTo(dest))
        return;

    m_activeStep.origin      = m_tilePosition;
    m_activeStep.destination = dest;
    m_activeStep.direction   = intent.direction;
    m_activeStep.locomotion  = intent.locomotion;
    m_activeStep.durationMs  = (intent.locomotion == Locomotion::Running)
                                ? RUN_DURATION_MS : WALK_DURATION_MS;
    m_activeStep.elapsedMs   = 0.0f;
    m_activeStep.active      = true;
}

void PlayerMovementSimulation::resolveNextIntent()
{
    if (!m_pendingIntent.active)
        return;

    auto off = directionOffset(m_pendingIntent.direction);
    GridCoord dest = m_tilePosition + off;

    if (m_world && m_world->canStepTo(dest))
        commitStep(m_pendingIntent);
}

PlayerMovementSimulation::PresentationData
PlayerMovementSimulation::presentation(float tileSize) const
{
    PresentationData d{};
    d.facing     = m_activeStep.active ? m_activeStep.direction : m_pendingIntent.direction;
    d.locomotion = m_activeStep.active ? m_activeStep.locomotion : Locomotion::Standing;
    d.stepProgress = m_activeStep.active ? m_activeStep.progress() : 0.0f;
    d.isMoving   = m_activeStep.active;

    if (m_activeStep.active)
    {
        float t = smoothStep(m_activeStep.progress());
        float fromX = (m_activeStep.origin.x + 0.5f) * tileSize;
        float fromY = (m_activeStep.origin.y + 0.5f) * tileSize;
        float toX   = (m_activeStep.destination.x + 0.5f) * tileSize;
        float toY   = (m_activeStep.destination.y + 0.5f) * tileSize;
        d.pixelX = fromX + (toX - fromX) * t;
        d.pixelY = fromY + (toY - fromY) * t;
    }
    else
    {
        d.pixelX = (m_tilePosition.x + 0.5f) * tileSize;
        d.pixelY = (m_tilePosition.y + 0.5f) * tileSize;
    }

    return d;
}

} // namespace Simulation
