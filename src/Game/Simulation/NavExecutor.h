#pragma once

#include "Game/Input/PlayerCommand.h"
#include "Game/Simulation/GreedyNavigator.h"
#include "Game/Simulation/MoveIntent.h"
#include "Game/Simulation/TargetResolver.h"
#include "Game/Simulation/TargetWorld.h"

#include <optional>

// NavExecutor: turns an engaged ResolvedTarget into a stream of PlayerCommands
// driven by the committed-step cadence of the caller (Game loop). It is the
// "act when reached" tail of the agreed pipeline:
//
//   SetTarget → TargetResolver → ResolvedTarget → GreedyNavigator → NavExecutor
//       → Move{dir,loco} (one per committed step) → Attack when reached
//
// Caller contract: call `nextMove` once per committed step. Manual joystick
// suspends it (`setSuspended(true)`); the target is kept and the tail resumes
// on release, re-evaluating from whatever tile the player ended up on.

namespace Simulation {

class NavExecutor {
public:
    void engage(const ResolvedTarget& target)
    {
        m_navigator.setTarget(target);
        m_actionSent = false;
        m_status = GreedyNavigator::Status::Idle;
    }

    void disengage()
    {
        m_navigator.clear();
        m_actionSent = false;
        m_status = GreedyNavigator::Status::Idle;
    }

    bool isEngaged() const { return m_navigator.hasTarget(); }
    const ResolvedTarget* target() const { return m_navigator.target(); }

    // Manual input active: stop emitting, keep the target for later resume.
    void setSuspended(bool s)
    {
        if (m_suspended && !s)
        {
            // Re-arm the navigator's history: the player moved manually, so the
            // pre-suspend tile is no longer a meaningful "where we came from".
            if (const ResolvedTarget* t = m_navigator.target())
                m_navigator.setTarget(*t);
        }
        m_suspended = s;
    }
    bool isSuspended() const { return m_suspended; }

    // Run-state applied to nav-generated moves (producer pushes the current
    // hold-to-run state here; default Walking).
    void setLocomotion(Locomotion loco) { m_locomotion = loco; }
    Locomotion locomotion() const { return m_locomotion; }

    GreedyNavigator::Status status() const { return m_status; }

    struct NavResult {
        GreedyNavigator::Status status = GreedyNavigator::Status::Idle;
        std::optional<Input::PlayerMove> move;      // set when Moving | Blocked
        std::optional<Input::PlayerCommand> action; // once, when Reached + Attack
    };

    NavResult nextMove(const ITargetWorld& world, GridCoord from)
    {
        NavResult out;

        if (!m_navigator.hasTarget() || m_suspended)
        {
            out.status = GreedyNavigator::Status::Idle;
            m_status = out.status;
            return out;
        }

        const GreedyNavigator::Result step = m_navigator.next(world, from);
        out.status = step.status;
        m_status = step.status;

        if (step.status == GreedyNavigator::Status::Moving ||
            step.status == GreedyNavigator::Status::Blocked)
        {
            out.move = Input::PlayerMove{ step.direction, m_locomotion };
            m_actionSent = false; // a fresh approach re-arms the reach-action
            return out;
        }

        if (step.status == GreedyNavigator::Status::Reached)
        {
            if (const ResolvedTarget* t = m_navigator.target())
                if (t->verb == Input::TargetVerb::Attack && t->hasTargetId &&
                    !m_actionSent)
                {
                    m_actionSent = true; // one-shot; caller re-engages to retry
                    out.action = Input::PlayerCommand(
                        Input::PlayerAttack{ t->targetId, false });
                }
        }
        return out;
    }

private:
    GreedyNavigator m_navigator;
    bool m_suspended = false;
    Locomotion m_locomotion = Locomotion::Walking;
    bool m_actionSent = false;
    GreedyNavigator::Status m_status = GreedyNavigator::Status::Idle;
};

// Boundary bridge: Input::PlayerMove (the command) → MoveIntent (the sim's
// internal move request). This is the only seam between the two vocabularies.
inline MoveIntent toMoveIntent(const Input::PlayerMove& mv)
{
    MoveIntent intent;
    intent.direction = mv.direction;
    intent.locomotion = mv.locomotion;
    intent.active = true;
    return intent;
}

} // namespace Simulation