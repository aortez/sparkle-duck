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

    // Create new SimRunning state with world.
    SimRunning newState;

    // Create world immediately.
    spdlog::info("Idle: Creating new World {}x{}", dsm.defaultWidth, dsm.defaultHeight);
    newState.world = std::make_unique<World>(dsm.defaultWidth, dsm.defaultHeight);

    // Lookup and apply scenario.
    auto& registry = dsm.getScenarioRegistry();
    auto* scenario = registry.getScenario(cwc.command.scenario_id);

    if (!scenario) {
        spdlog::error("Idle: Scenario '{}' not found in registry", cwc.command.scenario_id);
        cwc.sendResponse(Api::SimRun::Response::error(
            ApiError("Scenario not found: " + cwc.command.scenario_id)));
        // Return to Idle state (don't transition).
        return Idle{};
    }

    // Apply scenario's WorldEventGenerator.
    auto generator = scenario->createWorldEventGenerator();
    newState.world->setWorldEventGenerator(std::move(generator));

    // Populate WorldData with scenario metadata and config.
    newState.world->data.scenario_id = cwc.command.scenario_id;
    newState.world->data.scenario_config = scenario->getConfig();

    // Run setup to initialize scenario features.
    newState.world->setup();

    spdlog::info("Idle: Scenario '{}' applied to new world", cwc.command.scenario_id);

    // Set run parameters.
    newState.stepDurationMs = cwc.command.timestep * 1000.0; // Convert seconds to milliseconds.
    newState.targetSteps =
        cwc.command.max_steps > 0 ? static_cast<uint32_t>(cwc.command.max_steps) : 0;
    newState.stepCount = 0;
    newState.useRealtime = cwc.command.use_realtime;

    spdlog::info(
        "Idle: World created, transitioning to SimRunning (timestep={}ms, max_steps={}, "
        "use_realtime={})",
        newState.stepDurationMs,
        cwc.command.max_steps,
        newState.useRealtime);

    // Send response immediately (before transition).
    cwc.sendResponse(Api::SimRun::Response::okay({ true, 0 }));

    // Transition to SimRunning.
    return newState;
}

} // namespace State
} // namespace Server
} // namespace DirtSim
