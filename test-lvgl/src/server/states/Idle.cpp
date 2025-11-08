#include "server/StateMachine.h"
#include "core/World.h"
#include "State.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Server {
namespace State {

void Idle::onEnter(StateMachine& /*dsm*/)
{
    spdlog::info("Idle: Server ready, waiting for commands (no active World)");
    // Note: World is owned by SimRunning state, not StateMachine.
}

void Idle::onExit(StateMachine& /*dsm*/)
{
    spdlog::info("Idle: Exiting");
}

State::Any Idle::onEvent(const Api::Exit::Cwc& cwc, StateMachine& /*dsm*/)
{
    spdlog::info("Idle: Exit command received, shutting down");

    // Send success response.
    cwc.sendResponse(Api::Exit::Response::okay(std::monostate{}));

    // Transition to Shutdown state (Shutdown.onEnter will set shouldExit flag).
    return Shutdown{};
}

State::Any Idle::onEvent(const Api::SimRun::Cwc& cwc, StateMachine& dsm)
{
    spdlog::info("Idle: SimRun command received, creating world and starting simulation");

    // Create new SimRunning state with world.
    SimRunning newState;

    // Create world immediately.
    spdlog::info("Idle: Creating new World {}x{}", dsm.defaultWidth, dsm.defaultHeight);
    newState.world = std::make_unique<World>(dsm.defaultWidth, dsm.defaultHeight);

    // Set run parameters.
    newState.stepDurationMs = cwc.command.timestep * 1000.0;  // Convert seconds to milliseconds.
    newState.targetSteps = cwc.command.max_steps > 0 ? static_cast<uint32_t>(cwc.command.max_steps) : 0;
    newState.stepCount = 0;

    spdlog::info("Idle: World created, transitioning to SimRunning (timestep={}ms, max_steps={})",
                 newState.stepDurationMs, cwc.command.max_steps);

    // Send response immediately (before transition).
    cwc.sendResponse(Api::SimRun::Response::okay({true, 0}));

    // Transition to SimRunning.
    return newState;
}

} // namespace State
} // namespace Server
} // namespace DirtSim
