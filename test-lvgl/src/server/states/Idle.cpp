#include "State.h"
#include "core/World.h"
#include "server/StateMachine.h"
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
    spdlog::info(
        "Idle: SimRun command received, creating world with scenario '{}'",
        cwc.command.scenario_id);

    // Validate max_frame_ms parameter.
    if (cwc.command.max_frame_ms < 0) {
        spdlog::error("Idle: Invalid max_frame_ms value: {}", cwc.command.max_frame_ms);
        cwc.sendResponse(Api::SimRun::Response::error(
            ApiError("max_frame_ms must be >= 0 (0 = unlimited, >0 = frame rate cap)")));
        return Idle{};
    }

    // Create new SimRunning state with world.
    SimRunning newState;

    // Create world immediately.
    spdlog::info("Idle: Creating new World {}x{}", dsm.defaultWidth, dsm.defaultHeight);
    newState.world = std::make_unique<World>(dsm.defaultWidth, dsm.defaultHeight);

    // Create scenario instance from factory.
    auto& registry = dsm.getScenarioRegistry();
    newState.scenario = registry.createScenario(cwc.command.scenario_id);

    if (!newState.scenario) {
        spdlog::error("Idle: Scenario '{}' not found in registry", cwc.command.scenario_id);
        cwc.sendResponse(Api::SimRun::Response::error(
            ApiError("Scenario not found: " + cwc.command.scenario_id)));
        // Return to Idle state (don't transition).
        return Idle{};
    }

    // Populate WorldData with scenario metadata and config.
    newState.world->data.scenario_id = cwc.command.scenario_id;
    newState.world->data.scenario_config = newState.scenario->getConfig();

    // Run scenario setup to initialize world.
    newState.scenario->setup(*newState.world);

    spdlog::info("Idle: Scenario '{}' applied to new world", cwc.command.scenario_id);

    // Set run parameters.
    newState.stepDurationMs = cwc.command.timestep * 1000.0; // Convert seconds to milliseconds.
    newState.targetSteps =
        cwc.command.max_steps > 0 ? static_cast<uint32_t>(cwc.command.max_steps) : 0;
    newState.stepCount = 0;
    newState.frameLimit = cwc.command.max_frame_ms;

    spdlog::info(
        "Idle: World created, transitioning to SimRunning (timestep={}ms, max_steps={}, "
        "max_frame_ms={})",
        newState.stepDurationMs,
        cwc.command.max_steps,
        newState.frameLimit);

    // Send response immediately (before transition).
    cwc.sendResponse(Api::SimRun::Response::okay({ true, 0 }));

    // Transition to SimRunning.
    return newState;
}

} // namespace State
} // namespace Server
} // namespace DirtSim
