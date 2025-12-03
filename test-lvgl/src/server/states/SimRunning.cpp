#include "State.h"
#include "core/Cell.h"
#include "core/Timers.h"
#include "core/World.h" // Must be before State.h for complete type.
#include "core/WorldFrictionCalculator.h"
#include "core/organisms/TreeManager.h"
#include "server/StateMachine.h"
#include "server/network/WebSocketServer.h"
#include "server/scenarios/Scenario.h"
#include "server/scenarios/ScenarioRegistry.h"
#include <chrono>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <zpp_bits.h>

namespace DirtSim {
namespace Server {
namespace State {

void SimRunning::onEnter(StateMachine& dsm)
{
    spdlog::info("SimRunning: Entering simulation state");

    // Create World if it doesn't exist (first time entering from Idle).
    if (!world) {
        spdlog::info("SimRunning: Creating new World {}x{}", dsm.defaultWidth, dsm.defaultHeight);
        world = std::make_unique<World>(dsm.defaultWidth, dsm.defaultHeight);
    }
    else {
        spdlog::info(
            "SimRunning: Resuming with existing World {}x{}",
            world->getData().width,
            world->getData().height);
    }

    // Apply default "sandbox" scenario if no scenario is set.
    if (world && world->getData().scenario_id == "empty") {
        spdlog::info("SimRunning: Applying default 'sandbox' scenario");

        auto& registry = dsm.getScenarioRegistry();
        scenario = registry.createScenario("sandbox");

        if (scenario) {
            // Populate WorldData with scenario metadata and config.
            world->getData().scenario_id = "sandbox";
            world->getData().scenario_config = scenario->getConfig();

            // Clear world before applying scenario.
            for (uint32_t y = 0; y < world->getData().height; ++y) {
                for (uint32_t x = 0; x < world->getData().width; ++x) {
                    world->getData().at(x, y) = Cell(); // Reset to empty cell.
                }
            }

            // Run scenario setup to initialize world.
            scenario->setup(*world);

            spdlog::info("SimRunning: Default scenario 'sandbox' applied");
        }
    }

    spdlog::info("SimRunning: Ready to run simulation (stepCount={})", stepCount);
}

void SimRunning::onExit(StateMachine& /*dsm. */)
{
    spdlog::info("SimRunning: Exiting state");
}

void SimRunning::tick(StateMachine& dsm)
{
    // Check if we've reached target steps.
    if (targetSteps > 0 && stepCount >= targetSteps) {
        spdlog::debug("SimRunning: Reached target steps ({}), not advancing", targetSteps);
        return;
    }

    // Headless server: advance physics simulation with fixed timestep accumulator.
    assert(world && "World must exist in SimRunning state");

    // Measure real elapsed time since last physics update.
    const auto now = std::chrono::steady_clock::now();

    // Scenario tick (particle generation, timed events, etc.).
    if (scenario) {
        dsm.getTimers().startTimer("scenario_tick");
        scenario->tick(*world, FIXED_TIMESTEP_SECONDS);
        dsm.getTimers().stopTimer("scenario_tick");

        // Sync scenario's config to WorldData (scenario is source of truth).
        // This ensures auto-changes (like water column auto-disable) propagate to UI.
        world->getData().scenario_config = scenario->getConfig();
    }

    // Advance physics by fixed timestep.
    dsm.getTimers().startTimer("physics_step");
    world->advanceTime(FIXED_TIMESTEP_SECONDS);
    dsm.getTimers().stopTimer("physics_step");

    stepCount++;

    // Calculate actual FPS (physics steps per second).
    const auto frameElapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(now - lastFrameTime).count();
    if (frameElapsed > 0) {
        actualFPS = 1000000.0 / frameElapsed;    // Microseconds to FPS.
        world->getData().fps_server = actualFPS; // Update WorldData for UI.
        lastFrameTime = now;

        // Log FPS and performance stats intermittently.
        if (stepCount == 100 || stepCount % 500 == 0) {
            spdlog::info("SimRunning: Actual FPS: {:.1f} (step {})", actualFPS, stepCount);

            // Log performance timing stats.
            auto& timers = dsm.getTimers();
            spdlog::info(
                "  Physics: {:.1f}ms avg ({} calls, {:.1f}ms total)",
                timers.getCallCount("physics_step") > 0 ? timers.getAccumulatedTime("physics_step")
                        / timers.getCallCount("physics_step")
                                                        : 0.0,
                timers.getCallCount("physics_step"),
                timers.getAccumulatedTime("physics_step"));
            spdlog::info(
                "  Cache update: {:.1f}ms avg ({} calls, {:.1f}ms total)",
                timers.getCallCount("cache_update") > 0 ? timers.getAccumulatedTime("cache_update")
                        / timers.getCallCount("cache_update")
                                                        : 0.0,
                timers.getCallCount("cache_update"),
                timers.getAccumulatedTime("cache_update"));
            spdlog::info(
                "  zpp_bits pack: {:.2f}ms avg ({} calls, {:.1f}ms total)",
                timers.getCallCount("serialize_worlddata") > 0
                    ? timers.getAccumulatedTime("serialize_worlddata")
                        / timers.getCallCount("serialize_worlddata")
                    : 0.0,
                timers.getCallCount("serialize_worlddata"),
                timers.getAccumulatedTime("serialize_worlddata"));
            spdlog::info(
                "  Network send: {:.2f}ms avg ({} calls, {:.1f}ms total)",
                timers.getCallCount("network_send") > 0 ? timers.getAccumulatedTime("network_send")
                        / timers.getCallCount("network_send")
                                                        : 0.0,
                timers.getCallCount("network_send"),
                timers.getAccumulatedTime("network_send"));
            spdlog::info(
                "  state_get immediate (total): {:.2f}ms avg ({} calls, {:.1f}ms total)",
                timers.getCallCount("state_get_immediate_total") > 0
                    ? timers.getAccumulatedTime("state_get_immediate_total")
                        / timers.getCallCount("state_get_immediate_total")
                    : 0.0,
                timers.getCallCount("state_get_immediate_total"),
                timers.getAccumulatedTime("state_get_immediate_total"));
        }
    }

    // Populate tree vision data (if any trees exist).
    const auto& trees = world->getTreeManager().getTrees();
    if (!trees.empty()) {
        // For now, show the first tree's vision (simple selection).
        const auto& firstTree = trees.begin()->second;
        world->getData().tree_vision = firstTree.gatherSensoryData(*world);

        if (stepCount % 100 == 0) {
            spdlog::info(
                "SimRunning: Tree vision active (tree_id={}, age_seconds={}, stage={})",
                firstTree.id,
                firstTree.age_seconds,
                static_cast<int>(firstTree.stage));
        }
    }
    else {
        // No trees - clear tree vision.
        world->getData().tree_vision.reset();
    }

    // Update StateMachine's cached WorldData after all physics steps complete.
    dsm.getTimers().startTimer("cache_update");
    dsm.updateCachedWorldData(world->getData());
    dsm.getTimers().stopTimer("cache_update");

    spdlog::debug("SimRunning: Advanced simulation, total step {})", stepCount);

    // Send frame to UI clients after every physics update.
    // UI frame dropping handles overflow if rendering can't keep up.
    if (dsm.getWebSocketServer()) {
        auto& timers = dsm.getTimers();

        // Broadcast RenderMessage to all clients (per-client format).
        auto broadcastStart = std::chrono::steady_clock::now();
        timers.startTimer("broadcast_render_message");
        dsm.getWebSocketServer()->broadcastRenderMessage(*world);
        timers.stopTimer("broadcast_render_message");
        auto broadcastEnd = std::chrono::steady_clock::now();
        auto broadcastMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(broadcastEnd - broadcastStart)
                .count();

        static int sendCount = 0;
        static double totalBroadcastMs = 0.0;
        sendCount++;
        totalBroadcastMs += broadcastMs;
        if (sendCount % 1000 == 0) {
            spdlog::info(
                "Server: RenderMessage broadcast avg {:.1f}ms over {} frames (latest: {}ms, {} "
                "cells)",
                totalBroadcastMs / sendCount,
                sendCount,
                broadcastMs,
                world->getData().cells.size());
        }

        // Track FPS for frame send rate.
        auto now = std::chrono::steady_clock::now();
        if (lastFrameSendTime.time_since_epoch().count() > 0) {
            auto sendElapsed =
                std::chrono::duration_cast<std::chrono::microseconds>(now - lastFrameSendTime)
                    .count();
            if (sendElapsed > 0) {
                frameSendFPS = 1000000.0 / sendElapsed;
                world->getData().fps_server = frameSendFPS; // Update WorldData for UI display.
            }
        }
        lastFrameSendTime = now;
    }
}

State::Any SimRunning::onEvent(const ApplyScenarioCommand& cmd, StateMachine& dsm)
{
    spdlog::info("SimRunning: Applying scenario: {}", cmd.scenarioName);

    auto& registry = dsm.getScenarioRegistry();
    scenario = registry.createScenario(cmd.scenarioName);

    if (!scenario) {
        spdlog::error("Scenario not found: {}", cmd.scenarioName);
        return std::move(*this);
    }

    if (world) {
        // Populate WorldData with scenario metadata and config.
        world->getData().scenario_id = cmd.scenarioName;
        world->getData().scenario_config = scenario->getConfig();

        spdlog::info("SimRunning: Scenario '{}' applied to WorldData", cmd.scenarioName);
    }

    return std::move(*this);
}
State::Any SimRunning::onEvent(const Api::CellGet::Cwc& cwc, StateMachine& /*dsm*/)
{
    using Response = Api::CellGet::Response;

    // Validate coordinates.
    if (!world) {
        cwc.sendResponse(Response::error(ApiError("No world available")));
        return std::move(*this);
    }

    if (cwc.command.x < 0 || cwc.command.y < 0
        || static_cast<uint32_t>(cwc.command.x) >= world->getData().width
        || static_cast<uint32_t>(cwc.command.y) >= world->getData().height) {
        cwc.sendResponse(Response::error(ApiError("Invalid coordinates")));
        return std::move(*this);
    }

    // Get cell.
    const Cell& cell = world->getData().at(cwc.command.x, cwc.command.y);

    cwc.sendResponse(Response::okay({ cell }));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const Api::DiagramGet::Cwc& cwc, StateMachine& /*dsm*/)
{
    using Response = Api::DiagramGet::Response;

    assert(world && "World must exist in SimRunning state");

    // Get ASCII diagram from world.
    std::string diagram = world->toAsciiDiagram();

    spdlog::info("DiagramGet: Generated diagram ({} bytes):\n{}", diagram.size(), diagram);

    cwc.sendResponse(Response::okay({ diagram }));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const Api::CellSet::Cwc& cwc, StateMachine& /*dsm*/)
{
    using Response = Api::CellSet::Response;

    // Validate world availability.
    if (!world) {
        cwc.sendResponse(Response::error(ApiError("No world available")));
        return std::move(*this);
    }

    // Validate coordinates.
    if (cwc.command.x < 0 || cwc.command.y < 0
        || static_cast<uint32_t>(cwc.command.x) >= world->getData().width
        || static_cast<uint32_t>(cwc.command.y) >= world->getData().height) {
        cwc.sendResponse(Response::error(ApiError("Invalid coordinates")));
        return std::move(*this);
    }

    // Validate fill ratio.
    if (cwc.command.fill < 0.0 || cwc.command.fill > 1.0) {
        cwc.sendResponse(Response::error(ApiError("Fill must be between 0.0 and 1.0")));
        return std::move(*this);
    }

    // Place material.
    world->addMaterialAtCell(cwc.command.x, cwc.command.y, cwc.command.material, cwc.command.fill);

    cwc.sendResponse(Response::okay(std::monostate{}));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const Api::GravitySet::Cwc& cwc, StateMachine& /*dsm*/)
{
    using Response = Api::GravitySet::Response;

    if (!world) {
        cwc.sendResponse(Response::error(ApiError("No world available")));
        return std::move(*this);
    }

    world->getPhysicsSettings().gravity = cwc.command.gravity;
    spdlog::info("SimRunning: API set gravity to {}", cwc.command.gravity);

    cwc.sendResponse(Response::okay(std::monostate{}));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const Api::PerfStatsGet::Cwc& cwc, StateMachine& dsm)
{
    using Response = Api::PerfStatsGet::Response;

    // Gather performance statistics from timers.
    auto& timers = dsm.getTimers();

    Api::PerfStatsGet::Okay stats;
    stats.fps = actualFPS;

    // Physics timing.
    stats.physics_calls = timers.getCallCount("physics_step");
    stats.physics_total_ms = timers.getAccumulatedTime("physics_step");
    stats.physics_avg_ms =
        stats.physics_calls > 0 ? stats.physics_total_ms / stats.physics_calls : 0.0;

    // Serialization timing.
    stats.serialization_calls = timers.getCallCount("serialize_worlddata");
    stats.serialization_total_ms = timers.getAccumulatedTime("serialize_worlddata");
    stats.serialization_avg_ms = stats.serialization_calls > 0
        ? stats.serialization_total_ms / stats.serialization_calls
        : 0.0;

    // Cache update timing.
    stats.cache_update_calls = timers.getCallCount("cache_update");
    stats.cache_update_total_ms = timers.getAccumulatedTime("cache_update");
    stats.cache_update_avg_ms =
        stats.cache_update_calls > 0 ? stats.cache_update_total_ms / stats.cache_update_calls : 0.0;

    // Network send timing.
    stats.network_send_calls = timers.getCallCount("network_send");
    stats.network_send_total_ms = timers.getAccumulatedTime("network_send");
    stats.network_send_avg_ms =
        stats.network_send_calls > 0 ? stats.network_send_total_ms / stats.network_send_calls : 0.0;

    spdlog::info(
        "SimRunning: API perf_stats_get returning {} physics steps, {} serializations",
        stats.physics_calls,
        stats.serialization_calls);

    cwc.sendResponse(Response::okay(std::move(stats)));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const Api::TimerStatsGet::Cwc& cwc, StateMachine& /*dsm*/)
{
    using Response = Api::TimerStatsGet::Response;

    // Gather detailed timer statistics from World's timers.
    Api::TimerStatsGet::Okay stats;

    if (world) {
        auto timerNames = world->getTimers().getAllTimerNames();
        for (const auto& name : timerNames) {
            Api::TimerStatsGet::TimerEntry entry;
            entry.total_ms = world->getTimers().getAccumulatedTime(name);
            entry.calls = world->getTimers().getCallCount(name);
            entry.avg_ms = entry.calls > 0 ? entry.total_ms / entry.calls : 0.0;
            stats.timers[name] = entry;
        }
    }

    spdlog::info("SimRunning: API timer_stats_get returning {} timer entries", stats.timers.size());

    cwc.sendResponse(Response::okay(std::move(stats)));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const Api::StatusGet::Cwc& cwc, StateMachine& /*dsm*/)
{
    using Response = Api::StatusGet::Response;

    if (!world) {
        cwc.sendResponse(Response::error(ApiError("No world available")));
        return std::move(*this);
    }

    // Return lightweight status (no cell data).
    Api::StatusGet::Okay status;
    status.timestep = stepCount;
    status.scenario_id = world->getData().scenario_id;
    status.width = world->getData().width;
    status.height = world->getData().height;

    spdlog::debug(
        "SimRunning: API status_get (step {}, {}x{})",
        status.timestep,
        status.width,
        status.height);

    cwc.sendResponse(Response::okay(std::move(status)));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const Api::Reset::Cwc& cwc, StateMachine& /*dsm*/)
{
    using Response = Api::Reset::Response;

    spdlog::info("SimRunning: API reset simulation");

    if (world && scenario) {
        // Reset scenario (clears world and reinitializes).
        scenario->reset(*world);
    }

    stepCount = 0;

    cwc.sendResponse(Response::okay(std::monostate{}));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const Api::ScenarioConfigSet::Cwc& cwc, StateMachine& /*dsm*/)
{
    using Response = Api::ScenarioConfigSet::Response;

    spdlog::info("SimRunning: API update scenario config");

    if (!world) {
        cwc.sendResponse(Response::error(ApiError("No world available")));
        return std::move(*this);
    }

    if (!scenario) {
        spdlog::error("SimRunning: No scenario instance available");
        cwc.sendResponse(Response::error(ApiError("No scenario available")));
        return std::move(*this);
    }

    // Update scenario's config (scenario is source of truth).
    // Pass world so scenario can immediately apply changes.
    scenario->setConfig(cwc.command.config, *world);

    // Sync to WorldData (will be sent to UI on next frame).
    world->getData().scenario_config = scenario->getConfig();

    spdlog::info("SimRunning: Scenario config updated for '{}'", world->getData().scenario_id);

    cwc.sendResponse(Response::okay({ true }));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const Api::WorldResize::Cwc& cwc, StateMachine& /*dsm*/)
{
    using Response = Api::WorldResize::Response;

    const auto& cmd = cwc.command;
    spdlog::info("SimRunning: API resize world to {}x{}", cmd.width, cmd.height);

    if (world) {
        // Resize the world grid.
        world->resizeGrid(cmd.width, cmd.height);
        spdlog::debug("SimRunning: World resized successfully");
    }
    else {
        spdlog::error("SimRunning: Cannot resize - world is null");
        cwc.sendResponse(Response::error(ApiError("World not initialized")));
        return std::move(*this);
    }

    cwc.sendResponse(Response::okay(std::monostate{}));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const Api::SeedAdd::Cwc& cwc, StateMachine& /*dsm*/)
{
    using Response = Api::SeedAdd::Response;

    // Validate coordinates.
    if (cwc.command.x < 0 || cwc.command.y < 0
        || static_cast<uint32_t>(cwc.command.x) >= world->getData().width
        || static_cast<uint32_t>(cwc.command.y) >= world->getData().height) {
        cwc.sendResponse(Response::error(ApiError("Invalid coordinates")));
        return std::move(*this);
    }

    // Plant seed as tree organism.
    spdlog::info("SeedAdd: Planting seed at ({}, {})", cwc.command.x, cwc.command.y);
    TreeId tree_id = world->getTreeManager().plantSeed(*world, cwc.command.x, cwc.command.y);
    spdlog::info("SeedAdd: Created tree organism {}", tree_id);

    cwc.sendResponse(Response::okay(std::monostate{}));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const Api::SpawnDirtBall::Cwc& cwc, StateMachine& /*dsm*/)
{
    using Response = Api::SpawnDirtBall::Response;

    if (!world) {
        cwc.sendResponse(Response::error(ApiError("No world available")));
        return std::move(*this);
    }

    // Spawn a dirt ball at top center.
    uint32_t centerX = world->getData().width / 2;
    uint32_t topY = 2; // Start at row 2 to avoid the very top edge.

    spdlog::info("SpawnDirtBall: Spawning dirt ball at ({}, {})", centerX, topY);

    // Spawn a ball of the currently selected material.
    // Radius is calculated automatically as 15% of world width.
    MaterialType selectedMaterial = world->getSelectedMaterial();
    world->spawnMaterialBall(selectedMaterial, centerX, topY);

    cwc.sendResponse(Response::okay(std::monostate{}));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const Api::PhysicsSettingsGet::Cwc& cwc, StateMachine& /*dsm*/)
{
    using Response = Api::PhysicsSettingsGet::Response;

    if (!world) {
        cwc.sendResponse(Response::error(ApiError("No world available")));
        return std::move(*this);
    }

    spdlog::info("PhysicsSettingsGet: Sending current physics settings");

    Api::PhysicsSettingsGet::Okay okay;
    okay.settings = world->getPhysicsSettings();

    cwc.sendResponse(Response::okay(std::move(okay)));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const Api::PhysicsSettingsSet::Cwc& cwc, StateMachine& /*dsm*/)
{
    using Response = Api::PhysicsSettingsSet::Response;

    if (!world) {
        cwc.sendResponse(Response::error(ApiError("No world available")));
        return std::move(*this);
    }

    spdlog::info("PhysicsSettingsSet: Applying new physics settings");

    // Update world's physics settings (calculators read from this directly).
    world->getPhysicsSettings() = cwc.command.settings;

    cwc.sendResponse(Response::okay(std::monostate{}));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const Api::StateGet::Cwc& cwc, StateMachine& dsm)
{
    using Response = Api::StateGet::Response;

    // Track total server-side processing time.
    auto requestStart = std::chrono::steady_clock::now();

    if (!world) {
        cwc.sendResponse(Response::error(ApiError("No world available")));
        return std::move(*this);
    }

    // Return cached WorldData (fast - uses pre-cached copy, no World copy overhead!).
    auto cachedPtr = dsm.getCachedWorldData();
    if (cachedPtr) {
        Api::StateGet::Okay responseData;
        responseData.worldData = *cachedPtr;
        cwc.sendResponse(Response::okay(std::move(responseData)));
    }
    else {
        // Fallback: cache not ready yet, copy from world.
        Api::StateGet::Okay responseData;
        responseData.worldData = world->getData();
        cwc.sendResponse(Response::okay(std::move(responseData)));
    }

    // Log server processing time for state_get requests (includes serialization + send).
    auto requestEnd = std::chrono::steady_clock::now();
    double processingMs =
        std::chrono::duration<double, std::milli>(requestEnd - requestStart).count();
    spdlog::trace("SimRunning: state_get processed in {:.2f}ms (server-side total)", processingMs);

    return std::move(*this);
}

State::Any SimRunning::onEvent(const Api::SimRun::Cwc& cwc, StateMachine& dsm)
{
    using Response = Api::SimRun::Response;

    assert(world && "World must exist in SimRunning state");

    // Validate max_frame_ms parameter.
    if (cwc.command.max_frame_ms < 0) {
        spdlog::error("SimRunning: Invalid max_frame_ms value: {}", cwc.command.max_frame_ms);
        cwc.sendResponse(Response::error(
            ApiError("max_frame_ms must be >= 0 (0 = unlimited, >0 = frame rate cap)")));
        return std::move(*this);
    }

    // Check if scenario has changed - if so, reload the world.
    if (cwc.command.scenario_id != world->getData().scenario_id) {
        spdlog::info(
            "SimRunning: Switching scenario from '{}' to '{}'",
            world->getData().scenario_id,
            cwc.command.scenario_id);

        // Create new scenario instance from factory.
        auto& registry = dsm.getScenarioRegistry();
        scenario = registry.createScenario(cwc.command.scenario_id);

        if (!scenario) {
            spdlog::error(
                "SimRunning: Scenario '{}' not found in registry", cwc.command.scenario_id);
            cwc.sendResponse(
                Response::error(ApiError("Scenario not found: " + cwc.command.scenario_id)));
            return std::move(*this);
        }

        // Check if scenario requires specific world dimensions.
        const auto& metadata = scenario->getMetadata();
        uint32_t targetWidth, targetHeight;

        if (metadata.requiredWidth > 0 && metadata.requiredHeight > 0) {
            // Scenario requires specific dimensions.
            targetWidth = metadata.requiredWidth;
            targetHeight = metadata.requiredHeight;
        }
        else {
            // Scenario is flexible - use default dimensions.
            targetWidth = dsm.defaultWidth;
            targetHeight = dsm.defaultHeight;
        }

        // Resize if needed.
        if (world->getData().width != targetWidth || world->getData().height != targetHeight) {
            spdlog::info(
                "SimRunning: Resizing world from {}x{} to {}x{} for scenario '{}'",
                world->getData().width,
                world->getData().height,
                targetWidth,
                targetHeight,
                cwc.command.scenario_id);
            world->resizeGrid(targetWidth, targetHeight);
        }

        // Update world data.
        world->getData().scenario_id = cwc.command.scenario_id;
        world->getData().scenario_config = scenario->getConfig();

        // Clear world before applying new scenario.
        for (uint32_t y = 0; y < world->getData().height; ++y) {
            for (uint32_t x = 0; x < world->getData().width; ++x) {
                world->getData().at(x, y) = Cell(); // Reset to empty cell.
            }
        }
        spdlog::info("SimRunning: World cleared for new scenario '{}'", cwc.command.scenario_id);

        // Initialize world with new scenario.
        scenario->setup(*world);

        // Reset step counter.
        stepCount = 0;

        spdlog::info("SimRunning: Scenario '{}' loaded and initialized", cwc.command.scenario_id);
    }

    // Store run parameters.
    stepDurationMs = cwc.command.timestep * 1000.0; // Convert seconds to milliseconds.
    targetSteps = cwc.command.max_steps > 0 ? static_cast<uint32_t>(cwc.command.max_steps) : 0;
    frameLimit = cwc.command.max_frame_ms;

    spdlog::info(
        "SimRunning: Starting autonomous simulation (timestep={}ms, max_steps={}, max_frame_ms={})",
        stepDurationMs,
        cwc.command.max_steps,
        frameLimit);

    // Send response indicating simulation is running.
    cwc.sendResponse(Response::okay({ true, stepCount }));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const PauseCommand& /*cmd*/, StateMachine& /*dsm. */)
{
    spdlog::info("SimRunning: Pausing at step {}", stepCount);

    // Move the current state into SimPaused.
    return SimPaused{ std::move(*this) };
}

State::Any SimRunning::onEvent(const ResetSimulationCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    spdlog::info("SimRunning: Resetting simulation");

    if (world && scenario) {
        scenario->reset(*world);
    }

    stepCount = 0;

    return std::move(*this); // Stay in SimRunning (move because unique_ptr).
}

State::Any SimRunning::onEvent(const MouseDownEvent& /*evt*/, StateMachine& /*dsm*/)
{
    spdlog::debug("SimRunning: Mouse events not handled by headless server");
    return std::move(*this);
}

State::Any SimRunning::onEvent(const MouseMoveEvent& /*evt*/, StateMachine& /*dsm*/)
{
    spdlog::debug("SimRunning: Mouse events not handled by headless server");
    return std::move(*this);
}

State::Any SimRunning::onEvent(const MouseUpEvent& /*evt*/, StateMachine& /*dsm*/)
{
    spdlog::debug("SimRunning: Mouse events not handled by headless server");
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SelectMaterialCommand& cmd, StateMachine& /*dsm*/)
{
    if (world) {
        world->setSelectedMaterial(cmd.material);
        spdlog::debug("SimRunning: Selected material {}", static_cast<int>(cmd.material));
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetTimescaleCommand& cmd, StateMachine& /*dsm*/)
{
    // Update world directly (source of truth).
    if (world) {
        world->getPhysicsSettings().timescale = cmd.timescale;
        spdlog::info("SimRunning: Set timescale to {}", cmd.timescale);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetElasticityCommand& cmd, StateMachine& /*dsm*/)
{
    // Update world directly (source of truth).
    if (world) {
        world->getPhysicsSettings().elasticity = cmd.elasticity;
        spdlog::info("SimRunning: Set elasticity to {}", cmd.elasticity);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetDynamicStrengthCommand& cmd, StateMachine& /*dsm*/)
{
    // Update world directly (source of truth).
    if (world) {
        world->getPhysicsSettings().pressure_dynamic_strength = cmd.strength;
        spdlog::info("SimRunning: Set dynamic strength to {:.1f}", cmd.strength);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetGravityCommand& cmd, StateMachine& /*dsm*/)
{
    // Update world directly (source of truth).
    if (world) {
        world->getPhysicsSettings().gravity = cmd.gravity;
        spdlog::info("SimRunning: Set gravity to {}", cmd.gravity);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetPressureScaleCommand& cmd, StateMachine& /*dsm*/)
{
    // Apply to world.
    if (world) {
        world->getPhysicsSettings().pressure_scale = cmd.scale;
    }

    spdlog::debug("SimRunning: Set pressure scale to {}", cmd.scale);
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetPressureScaleWorldBCommand& cmd, StateMachine& /*dsm*/)
{
    // Apply to world.
    if (world) {
        world->getPhysicsSettings().pressure_scale = cmd.scale;
    }

    spdlog::debug("SimRunning: Set World pressure scale to {}", cmd.scale);
    return std::move(*this);
}

// Obsolete individual strength commands removed - use PhysicsSettingsSet instead.
// These settings are now controlled via the unified PhysicsSettings API.

State::Any SimRunning::onEvent(const SetContactFrictionStrengthCommand& cmd, StateMachine& /*dsm*/)
{
    if (auto* worldPtr = dynamic_cast<World*>(world.get())) {
        worldPtr->getPhysicsSettings().friction_strength = cmd.strength;
        spdlog::info("SimRunning: Set contact friction strength to {}", cmd.strength);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetCOMCohesionRangeCommand& cmd, StateMachine& /*dsm*/)
{
    if (world) {
        world->setCOMCohesionRange(cmd.range);
        spdlog::info("SimRunning: Set COM cohesion range to {}", cmd.range);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetAirResistanceCommand& cmd, StateMachine& /*dsm*/)
{
    if (world) {
        world->setAirResistanceStrength(cmd.strength);
        spdlog::info("SimRunning: Set air resistance to {}", cmd.strength);
    }
    return std::move(*this);
}

// Obsolete toggle commands removed - use PhysicsSettingsSet API instead.

State::Any SimRunning::onEvent(
    const SetHydrostaticPressureStrengthCommand& cmd, StateMachine& /*dsm*/)
{
    if (world) {
        world->getPhysicsSettings().pressure_hydrostatic_strength = cmd.strength;
        spdlog::info("SimRunning: Set hydrostatic pressure strength to {}", cmd.strength);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetDynamicPressureStrengthCommand& cmd, StateMachine& /*dsm*/)
{
    // Apply to world.
    if (world) {
        // TODO: Need to add setDynamicPressureStrength method to WorldInterface.
        // For now, suppress unused warning.
        (void)world;
    }

    spdlog::debug("SimRunning: Set dynamic pressure strength to {}", cmd.strength);
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetRainRateCommand& cmd, StateMachine& /*dsm*/)
{
    if (world && scenario) {
        ScenarioConfig config = scenario->getConfig();

        // Update rain_rate in whichever config variant supports it.
        if (auto* sandboxCfg = std::get_if<SandboxConfig>(&config)) {
            sandboxCfg->rain_rate = cmd.rate;
            scenario->setConfig(config, *world);
            spdlog::info("SimRunning: Set rain rate to {} (SandboxConfig)", cmd.rate);
        }
        else if (auto* rainingCfg = std::get_if<RainingConfig>(&config)) {
            rainingCfg->rain_rate = cmd.rate;
            scenario->setConfig(config, *world);
            spdlog::info("SimRunning: Set rain rate to {} (RainingConfig)", cmd.rate);
        }
        else {
            spdlog::warn("SimRunning: Current scenario does not support rain_rate");
        }
    }
    return std::move(*this);
}

// Handle immediate events routed through push system.
State::Any SimRunning::onEvent(const GetFPSCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    spdlog::debug("SimRunning: GetFPSCommand not implemented in headless server");
    // TODO: Track FPS if needed for headless operation.
    return std::move(*this);
}

State::Any SimRunning::onEvent(const GetSimStatsCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    spdlog::debug("SimRunning: GetSimStatsCommand not implemented in headless server");
    // TODO: Return simulation statistics if needed.
    return std::move(*this);
}

State::Any SimRunning::onEvent(const ToggleCohesionForceCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    if (world) {
        bool newValue = !world->isCohesionComForceEnabled();
        world->setCohesionComForceEnabled(newValue);
        spdlog::info("SimRunning: Cohesion force now: {}", newValue);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const ToggleTimeHistoryCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    if (world) {
        bool newValue = !world->isTimeReversalEnabled();
        world->enableTimeReversal(newValue);
        spdlog::info("SimRunning: Time history now: {}", newValue);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const PrintAsciiDiagramCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    // Get the current world and print ASCII diagram.
    if (world) {
        std::string ascii_diagram = world->toAsciiDiagram();
        spdlog::info("Current world state (ASCII diagram):\n{}", ascii_diagram);
    }
    else {
        spdlog::warn("PrintAsciiDiagramCommand: No world available");
    }

    return std::move(*this);
}

State::Any SimRunning::onEvent(const SpawnDirtBallCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    // Get the current world and spawn a ball at top center.
    if (world) {
        // Calculate the top center position.
        uint32_t centerX = world->getData().width / 2;
        uint32_t topY = 2; // Start at row 2 to avoid the very top edge.

        // Spawn a ball of the currently selected material.
        // Radius is calculated automatically as 15% of world width.
        MaterialType selectedMaterial = world->getSelectedMaterial();
        world->spawnMaterialBall(selectedMaterial, centerX, topY);
    }
    else {
        spdlog::warn("SpawnDirtBallCommand: No world available");
    }

    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetFragmentationCommand& cmd, StateMachine& /*dsm*/)
{
    if (world) {
        world->setDirtFragmentationFactor(cmd.factor);
        spdlog::info("SimRunning: Set fragmentation factor to {}", cmd.factor);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const ToggleWaterColumnCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    if (world && scenario) {
        ScenarioConfig config = scenario->getConfig();

        // Toggle water_column_enabled in SandboxConfig.
        if (auto* sandboxCfg = std::get_if<SandboxConfig>(&config)) {
            sandboxCfg->water_column_enabled = !sandboxCfg->water_column_enabled;
            scenario->setConfig(config, *world);
            spdlog::info(
                "SimRunning: Water column toggled - now: {}", sandboxCfg->water_column_enabled);
        }
        else {
            spdlog::warn("SimRunning: Current scenario does not support water column toggle");
        }
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const ToggleLeftThrowCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    // Note: Left throw is not currently in SandboxConfig - this command is deprecated.
    // Use ScenarioConfigSet API to modify scenario configs directly.
    spdlog::warn("SimRunning: ToggleLeftThrowCommand is deprecated - left throw not in config");
    return std::move(*this);
}

State::Any SimRunning::onEvent(const ToggleRightThrowCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    if (world && scenario) {
        ScenarioConfig config = scenario->getConfig();

        // Toggle right_throw_enabled in SandboxConfig.
        if (auto* sandboxCfg = std::get_if<SandboxConfig>(&config)) {
            sandboxCfg->right_throw_enabled = !sandboxCfg->right_throw_enabled;
            scenario->setConfig(config, *world);
            spdlog::info(
                "SimRunning: Right throw toggled - now: {}", sandboxCfg->right_throw_enabled);
        }
        else {
            spdlog::warn("SimRunning: Current scenario does not support right throw toggle");
        }
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const ToggleQuadrantCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    if (world && scenario) {
        ScenarioConfig config = scenario->getConfig();

        // Toggle quadrant_enabled in SandboxConfig.
        if (auto* sandboxCfg = std::get_if<SandboxConfig>(&config)) {
            sandboxCfg->quadrant_enabled = !sandboxCfg->quadrant_enabled;
            scenario->setConfig(config, *world);
            spdlog::info("SimRunning: Quadrant toggled - now: {}", sandboxCfg->quadrant_enabled);
        }
        else {
            spdlog::warn("SimRunning: Current scenario does not support quadrant toggle");
        }
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const ToggleFrameLimitCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    // TODO: Need to add toggleFrameLimit method to World.
    spdlog::info("SimRunning: Toggle frame limit");
    return std::move(*this);
}

State::Any SimRunning::onEvent(const QuitApplicationCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    spdlog::info("Server::SimRunning: Quit application requested");

    // TODO: Add CaptureScreenshotCommand that ui/StateMachine can handle.
    // Screenshots are UI concerns, not server concerns.

    // Transition to Shutdown state.
    return Shutdown{};
}

State::Any SimRunning::onEvent(const Api::Exit::Cwc& cwc, StateMachine& /*dsm*/)
{
    spdlog::info("SimRunning: Exit command received, shutting down");

    // Send success response.
    cwc.sendResponse(Api::Exit::Response::okay(std::monostate{}));

    // Transition to Shutdown state (Shutdown.onEnter will set shouldExit flag).
    return Shutdown{};
}

} // namespace State
} // namespace Server
} // namespace DirtSim
