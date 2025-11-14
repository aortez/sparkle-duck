#include "State.h"
#include "core/Cell.h"
#include "core/World.h" // Must be before State.h for complete type.
#include "core/WorldEventGenerator.h"
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
            world->data.width,
            world->data.height);
    }

    // Apply default "sandbox" scenario if no scenario is set.
    if (world && world->data.scenario_id == "empty") {
        spdlog::info("SimRunning: Applying default 'sandbox' scenario");

        auto& registry = dsm.getScenarioRegistry();
        auto* scenario = registry.getScenario("sandbox");

        if (scenario) {
            // Apply scenario's WorldEventGenerator.
            auto setup = scenario->createWorldEventGenerator();
            world->setWorldEventGenerator(std::move(setup));

            // Populate WorldData with scenario metadata and config.
            world->data.scenario_id = "sandbox";
            world->data.scenario_config = scenario->getConfig();

            // Run setup to actually create scenario features.
            world->setup();

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

    // Advance physics by fixed timestep.
    dsm.getTimers().startTimer("physics_step");
    world->advanceTime(FIXED_TIMESTEP_SECONDS);
    dsm.getTimers().stopTimer("physics_step");

    stepCount++;

    // Calculate actual FPS (physics steps per second).
    const auto frameElapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(now - lastFrameTime).count();
    if (frameElapsed > 0) {
        actualFPS = 1000000.0 / frameElapsed; // Microseconds to FPS.
        world->data.fps_server = actualFPS;   // Update WorldData for UI.
        lastFrameTime = now;

        // Log FPS and performance stats intermittently.
        if (stepCount == 100 || stepCount % 1000 == 0) {
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
        world->data.tree_vision = firstTree.gatherSensoryData(*world);

        if (stepCount % 100 == 0) {
            spdlog::info(
                "SimRunning: Tree vision active (tree_id={}, age={}, stage={})",
                firstTree.id,
                firstTree.age,
                static_cast<int>(firstTree.stage));
        }
    }
    else {
        // No trees - clear tree vision.
        world->data.tree_vision.reset();
    }

    // Update StateMachine's cached WorldData after all physics steps complete.
    dsm.getTimers().startTimer("cache_update");
    dsm.updateCachedWorldData(world->data);
    dsm.getTimers().stopTimer("cache_update");

    spdlog::debug("SimRunning: Advanced simulation, total step {})", stepCount);

    // Send frame to UI clients after every physics update.
    // UI frame dropping handles overflow if rendering can't keep up.
    if (dsm.getWebSocketServer()) {
        auto& timers = dsm.getTimers();

        // Pack WorldData to binary.
        auto serializeStart = std::chrono::steady_clock::now();
        timers.startTimer("serialize_worlddata");
        std::vector<std::byte> data;
        zpp::bits::out out(data);
        out(world->data).or_throw();
        timers.stopTimer("serialize_worlddata");
        auto serializeEnd = std::chrono::steady_clock::now();
        auto serializeMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(serializeEnd - serializeStart)
                .count();

        // Broadcast binary WorldData to all clients.
        rtc::binary binaryMsg(data.begin(), data.end());

        static int sendCount = 0;
        static double totalSerializeMs = 0.0;
        sendCount++;
        totalSerializeMs += serializeMs;
        if (sendCount % 1000 == 0) {
            spdlog::info(
                "Server: Serialization avg {:.1f}ms over {} frames (latest: {}ms, {} bytes, {} "
                "cells)",
                totalSerializeMs / sendCount,
                sendCount,
                serializeMs,
                data.size(),
                world->data.cells.size());
        }

        auto networkStart = std::chrono::steady_clock::now();
        timers.startTimer("network_send");
        dsm.getWebSocketServer()->broadcastBinary(binaryMsg);
        timers.stopTimer("network_send");
        auto networkEnd = std::chrono::steady_clock::now();
        auto networkUs =
            std::chrono::duration_cast<std::chrono::microseconds>(networkEnd - networkStart)
                .count();

        if (networkUs > 10000) {
            spdlog::info(
                "SimRunning: Network send took {:.1f}ms for {} bytes",
                networkUs / 1000.0,
                data.size());
        }

        // Track FPS.
        auto now = std::chrono::steady_clock::now();
        if (lastFrameSendTime.time_since_epoch().count() > 0) {
            auto sendElapsed =
                std::chrono::duration_cast<std::chrono::microseconds>(now - lastFrameSendTime)
                    .count();
            if (sendElapsed > 0) {
                frameSendFPS = 1000000.0 / sendElapsed;
                world->data.fps_server = frameSendFPS; // Update WorldData for UI display.
            }
        }
        lastFrameSendTime = now;

        spdlog::debug(
            "SimRunning: Sent frame to UI ({} bytes, network {:.2f}ms, send FPS: {:.1f})",
            data.size(),
            networkUs / 1000.0,
            frameSendFPS);
    }
}

State::Any SimRunning::onEvent(const ApplyScenarioCommand& cmd, StateMachine& dsm)
{
    spdlog::info("SimRunning: Applying scenario: {}", cmd.scenarioName);

    auto& registry = dsm.getScenarioRegistry();
    auto* scenario = registry.getScenario(cmd.scenarioName);

    if (!scenario) {
        spdlog::error("Scenario not found: {}", cmd.scenarioName);
        return std::move(*this);
    }

    // TODO: Handle scenario-specific world resizing if needed.

    // Apply scenario's WorldEventGenerator.
    auto setup = scenario->createWorldEventGenerator();
    if (world) {
        world->setWorldEventGenerator(std::move(setup));

        // Populate WorldData with scenario metadata and config.
        world->data.scenario_id = cmd.scenarioName;
        world->data.scenario_config = scenario->getConfig();

        spdlog::info("SimRunning: Scenario '{}' applied to WorldData", cmd.scenarioName);
    }

    return std::move(*this);
}

State::Any SimRunning::onEvent(const ResizeWorldCommand& cmd, StateMachine& /*dsm*/)
{
    spdlog::info("SimRunning: Resizing world to {}x{}", cmd.width, cmd.height);
    // TODO: Implement world resizing (world->resize or recreate world).
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
        || static_cast<uint32_t>(cwc.command.x) >= world->data.width
        || static_cast<uint32_t>(cwc.command.y) >= world->data.height) {
        cwc.sendResponse(Response::error(ApiError("Invalid coordinates")));
        return std::move(*this);
    }

    // Get cell.
    const Cell& cell = world->at(cwc.command.x, cwc.command.y);

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
        || static_cast<uint32_t>(cwc.command.x) >= world->data.width
        || static_cast<uint32_t>(cwc.command.y) >= world->data.height) {
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

    world->physicsSettings.gravity = cwc.command.gravity;
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

State::Any SimRunning::onEvent(const Api::Reset::Cwc& cwc, StateMachine& /*dsm*/)
{
    using Response = Api::Reset::Response;

    spdlog::info("SimRunning: API reset simulation");

    if (world) {
        // Clear world to empty state, then re-initialize with scenario.
        world->worldEventGenerator_->clear(*world);
        world->setup();
    }

    stepCount = 0;

    cwc.sendResponse(Response::okay(std::monostate{}));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const Api::FrameReady::Cwc& cwc, StateMachine& /*dsm*/)
{
    using Response = Api::FrameReady::Response;

    // frame_ready is sent by the UI after rendering a frame (pipelining optimization).
    // Currently a NO-OP: The server pushes frames continuously via sim_step events,
    // not in response to frame_ready. This command exists for potential future backpressure.
    spdlog::debug("SimRunning: Received frame_ready (no-op)");

    cwc.sendResponse(Response::okay(std::monostate{}));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const Api::ScenarioConfigSet::Cwc& cwc, StateMachine& dsm)
{
    using Response = Api::ScenarioConfigSet::Response;

    spdlog::info("SimRunning: API update scenario config");

    if (!world) {
        cwc.sendResponse(Response::error(ApiError("No world available")));
        return std::move(*this);
    }

    // Get current scenario from ScenarioRegistry.
    auto& registry = dsm.getScenarioRegistry();
    auto* scenario = registry.getScenario(world->data.scenario_id);

    if (!scenario) {
        spdlog::error("SimRunning: Scenario '{}' not found in registry", world->data.scenario_id);
        cwc.sendResponse(
            Response::error(ApiError("Scenario not found: " + world->data.scenario_id)));
        return std::move(*this);
    }

    // Apply new config to scenario.
    scenario->setConfig(cwc.command.config);

    // Recreate WorldEventGenerator with new config.
    auto newGenerator = scenario->createWorldEventGenerator();

    // Apply immediate visual toggles for sandbox scenario.
    if (std::holds_alternative<SandboxConfig>(cwc.command.config)) {
        const auto& sandboxConfig = std::get<SandboxConfig>(cwc.command.config);
        newGenerator->dirtQuadrantToggle(*world, sandboxConfig.quadrant_enabled);
        newGenerator->waterColumnToggle(*world, sandboxConfig.water_column_enabled);
    }

    world->setWorldEventGenerator(std::move(newGenerator));

    // Update WorldData with new config.
    world->data.scenario_config = cwc.command.config;

    spdlog::info("SimRunning: Scenario config updated for '{}'", world->data.scenario_id);

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
        || static_cast<uint32_t>(cwc.command.x) >= world->data.width
        || static_cast<uint32_t>(cwc.command.y) >= world->data.height) {
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
    uint32_t centerX = world->data.width / 2;
    uint32_t topY = 2; // Start at row 2 to avoid the very top edge.

    spdlog::info("SpawnDirtBall: Spawning dirt ball at ({}, {})", centerX, topY);

    // Spawn a ball of the currently selected material.
    MaterialType selectedMaterial = world->getSelectedMaterial();
    world->spawnMaterialBall(selectedMaterial, centerX, topY, 2);

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
    okay.settings = world->physicsSettings;

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

    // Update world's physics settings.
    world->physicsSettings = cwc.command.settings;

    // TODO: Apply settings to World calculators (timescale, gravity, etc.).
    // For now, just store them - actual application will be added next.

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
        responseData.worldData = world->data;
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
    if (cwc.command.scenario_id != world->data.scenario_id) {
        spdlog::info(
            "SimRunning: Switching scenario from '{}' to '{}'",
            world->data.scenario_id,
            cwc.command.scenario_id);

        // Validate scenario exists.
        auto& registry = dsm.getScenarioRegistry();
        auto* scenario = registry.getScenario(cwc.command.scenario_id);

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
        if (world->data.width != targetWidth || world->data.height != targetHeight) {
            spdlog::info(
                "SimRunning: Resizing world from {}x{} to {}x{} for scenario '{}'",
                world->data.width,
                world->data.height,
                targetWidth,
                targetHeight,
                cwc.command.scenario_id);
            world->resizeGrid(targetWidth, targetHeight);
        }

        // Clear current world.
        world->worldEventGenerator_->clear(*world);

        // Create and apply new WorldEventGenerator from scenario.
        auto newGenerator = scenario->createWorldEventGenerator();
        world->setWorldEventGenerator(std::move(newGenerator));

        // Update world data.
        world->data.scenario_id = cwc.command.scenario_id;
        world->data.scenario_config = scenario->getConfig();

        // Re-initialize world with new scenario.
        world->setup();

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

    if (world) {
        world->setup();
    }

    stepCount = 0;

    return std::move(*this); // Stay in SimRunning (move because unique_ptr).
}

State::Any SimRunning::onEvent(const SaveWorldCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    spdlog::warn("SimRunning: SaveWorld not implemented yet");
    // TODO: Implement world saving.
    return std::move(*this);
}

State::Any SimRunning::onEvent(const StepBackwardCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    spdlog::debug("SimRunning: Stepping simulation backward by one timestep");

    if (!world) {
        spdlog::warn("SimRunning: Cannot step backward - no world available");
        return std::move(*this);
    }

    // TODO: Implement world->goBackward() method for time reversal.
    spdlog::info("StepBackwardCommand: Time reversal not yet implemented");

    return std::move(*this); // Stay in SimRunning (move because unique_ptr).
}

State::Any SimRunning::onEvent(const StepForwardCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    if (!world) {
        spdlog::warn("SimRunning: Cannot step forward - no world available");
        return std::move(*this);
    }

    // TODO: Implement world->goForward() method for time reversal.
    spdlog::info("SimRunning: Step forward requested");

    return std::move(*this); // Stay in SimRunning (move because unique_ptr).
}

State::Any SimRunning::onEvent(const ToggleTimeReversalCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    if (!world) {
        spdlog::warn("SimRunning: Cannot toggle time reversal - no world available");
        return std::move(*this);
    }

    // TODO: Implement world->toggleTimeReversal() method.
    spdlog::info("SimRunning: Toggle time reversal requested");

    return std::move(*this); // Stay in SimRunning (move because unique_ptr).
}

State::Any SimRunning::onEvent(const LoadWorldCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    spdlog::warn("SimRunning: LoadWorld not implemented yet");
    // TODO: Implement world loading.
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetTimestepCommand& cmd, StateMachine& /*dsm*/)
{
    // TODO: Implement world->setTimestep() method when available.
    spdlog::debug("SimRunning: Set timestep to {}", cmd.timestep_value);
    return std::move(*this);
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
        world->physicsSettings.timescale = cmd.timescale;
        spdlog::info("SimRunning: Set timescale to {}", cmd.timescale);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetElasticityCommand& cmd, StateMachine& /*dsm*/)
{
    // Update world directly (source of truth).
    if (world) {
        world->physicsSettings.elasticity = cmd.elasticity;
        spdlog::info("SimRunning: Set elasticity to {}", cmd.elasticity);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetDynamicStrengthCommand& cmd, StateMachine& /*dsm*/)
{
    // Update world directly (source of truth).
    if (world) {
        world->setDynamicPressureStrength(cmd.strength);
        spdlog::info("SimRunning: Set dynamic strength to {:.1f}", cmd.strength);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetGravityCommand& cmd, StateMachine& /*dsm*/)
{
    // Update world directly (source of truth).
    if (world) {
        world->physicsSettings.gravity = cmd.gravity;
        spdlog::info("SimRunning: Set gravity to {}", cmd.gravity);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetPressureScaleCommand& cmd, StateMachine& /*dsm*/)
{
    // Apply to world.
    if (world) {
        world->physicsSettings.pressure_scale = cmd.scale;
    }

    spdlog::debug("SimRunning: Set pressure scale to {}", cmd.scale);
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetPressureScaleWorldBCommand& cmd, StateMachine& /*dsm*/)
{
    // Apply to world.
    if (world) {
        world->physicsSettings.pressure_scale = cmd.scale;
    }

    spdlog::debug("SimRunning: Set World pressure scale to {}", cmd.scale);
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetCohesionForceStrengthCommand& cmd, StateMachine& /*dsm*/)
{
    if (world) {
        world->setCohesionComForceStrength(cmd.strength);
        spdlog::info("SimRunning: Set cohesion force strength to {}", cmd.strength);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetAdhesionStrengthCommand& cmd, StateMachine& /*dsm*/)
{
    if (world) {
        world->setAdhesionStrength(cmd.strength);
        spdlog::info("SimRunning: Set adhesion strength to {}", cmd.strength);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetViscosityStrengthCommand& cmd, StateMachine& /*dsm*/)
{
    if (world) {
        world->setViscosityStrength(cmd.strength);
        spdlog::info("SimRunning: Set viscosity strength to {}", cmd.strength);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetFrictionStrengthCommand& cmd, StateMachine& /*dsm*/)
{
    if (world) {
        world->setFrictionStrength(cmd.strength);
        spdlog::info("SimRunning: Set friction strength to {}", cmd.strength);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const SetContactFrictionStrengthCommand& cmd, StateMachine& /*dsm*/)
{
    if (auto* worldPtr = dynamic_cast<World*>(world.get())) {
        worldPtr->getFrictionCalculator().setFrictionStrength(cmd.strength);
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

State::Any SimRunning::onEvent(
    const ToggleHydrostaticPressureCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    if (world) {
        bool newValue = !world->isHydrostaticPressureEnabled();
        world->setHydrostaticPressureEnabled(newValue);
        spdlog::info("SimRunning: Toggle hydrostatic pressure - now: {}", newValue);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const ToggleDynamicPressureCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    if (world) {
        bool newValue = !world->isDynamicPressureEnabled();
        world->setDynamicPressureEnabled(newValue);
        spdlog::info("SimRunning: Toggle dynamic pressure - now: {}", newValue);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const TogglePressureDiffusionCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    if (world) {
        bool newValue = !world->isPressureDiffusionEnabled();
        world->setPressureDiffusionEnabled(newValue);
        spdlog::info("SimRunning: Toggle pressure diffusion - now: {}", newValue);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(
    const SetHydrostaticPressureStrengthCommand& cmd, StateMachine& /*dsm*/)
{
    if (world) {
        world->setHydrostaticPressureStrength(cmd.strength);
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
    if (world) {
        world->setRainRate(cmd.rate);
        spdlog::info("SimRunning: Set rain rate to {}", cmd.rate);
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
    // Get the current world and spawn a 5x5 ball at top center.
    if (world) {
        // Calculate the top center position.
        uint32_t centerX = world->data.width / 2;
        uint32_t topY = 2; // Start at row 2 to avoid the very top edge.

        // Spawn a 5x5 ball of the currently selected material.
        MaterialType selectedMaterial = world->getSelectedMaterial();
        world->spawnMaterialBall(selectedMaterial, centerX, topY, 2);
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

State::Any SimRunning::onEvent(const ToggleWallsCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    // Apply to world.
    if (world) {
        // TODO: Need to add toggleWalls method to WorldInterface.
        // For now, suppress unused warning.
        (void)world;
    }

    spdlog::info("SimRunning: Toggle walls");
    return std::move(*this);
}

State::Any SimRunning::onEvent(const ToggleWaterColumnCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    if (world) {
        bool newValue = !world->isWaterColumnEnabled();
        world->setWaterColumnEnabled(newValue);

        // For World, we can manipulate cells directly.
        World* worldB = dynamic_cast<World*>(world.get());
        if (worldB) {
            if (newValue) {
                // Add water column (5 wide × 20 tall) on left side.
                spdlog::info("SimRunning: Adding water column (5 wide × 20 tall) at runtime");
                for (uint32_t y = 0; y < 20 && y < worldB->data.height; ++y) {
                    for (uint32_t x = 1; x <= 5 && x < worldB->data.width; ++x) {
                        Cell& cell = worldB->at(x, y);
                        // Only add water to non-wall cells.
                        if (!cell.isWall()) {
                            cell.material_type = MaterialType::WATER;
                            cell.setFillRatio(1.0);
                            cell.setCOM(Vector2d{ 0.0, 0.0 });
                            cell.velocity = Vector2d{ 0.0, 0.0 };
                        }
                    }
                }
            }
            else {
                // Remove water from column area (only water cells).
                spdlog::info("SimRunning: Removing water from water column area at runtime");
                for (uint32_t y = 0; y < 20 && y < worldB->data.height; ++y) {
                    for (uint32_t x = 1; x <= 5 && x < worldB->data.width; ++x) {
                        Cell& cell = worldB->at(x, y);
                        // Only clear water cells, leave walls and other materials.
                        if (cell.material_type == MaterialType::WATER && !cell.isWall()) {
                            cell.material_type = MaterialType::AIR;
                            cell.setFillRatio(0.0);
                            cell.setCOM(Vector2d{ 0.0, 0.0 });
                            cell.velocity = Vector2d{ 0.0, 0.0 };
                        }
                    }
                }
            }
        }

        spdlog::info("SimRunning: Water column toggled - now: {}", newValue);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const ToggleLeftThrowCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    if (world) {
        bool newValue = !world->isLeftThrowEnabled();
        world->setLeftThrowEnabled(newValue);
        spdlog::info("SimRunning: Toggle left throw - now: {}", newValue);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const ToggleRightThrowCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    if (world) {
        bool newValue = !world->isRightThrowEnabled();
        world->setRightThrowEnabled(newValue);
        spdlog::info("SimRunning: Toggle right throw - now: {}", newValue);
    }
    return std::move(*this);
}

State::Any SimRunning::onEvent(const ToggleQuadrantCommand& /*cmd*/, StateMachine& /*dsm*/)
{
    if (world) {
        bool newValue = !world->isLowerRightQuadrantEnabled();
        world->setLowerRightQuadrantEnabled(newValue);

        // For World, manipulate cells directly for immediate feedback.
        DirtSim::World* worldB = dynamic_cast<DirtSim::World*>(world.get());
        if (worldB) {
            uint32_t startX = worldB->data.width / 2;
            uint32_t startY = worldB->data.height / 2;

            if (newValue) {
                // Add dirt quadrant immediately.
                spdlog::info(
                    "SimRunning: Adding lower right quadrant ({}x{}) at runtime",
                    worldB->data.width - startX,
                    worldB->data.height - startY);
                for (uint32_t y = startY; y < worldB->data.height; ++y) {
                    for (uint32_t x = startX; x < worldB->data.width; ++x) {
                        Cell& cell = worldB->at(x, y);
                        // Only add dirt to non-wall cells.
                        if (!cell.isWall()) {
                            cell.material_type = MaterialType::DIRT;
                            cell.setFillRatio(1.0);
                            cell.setCOM(Vector2d{ 0.0, 0.0 });
                            cell.velocity = Vector2d{ 0.0, 0.0 };
                        }
                    }
                }
            }
            else {
                // Remove dirt from quadrant area (only dirt cells).
                spdlog::info("SimRunning: Removing dirt from lower right quadrant at runtime");
                for (uint32_t y = startY; y < worldB->data.height; ++y) {
                    for (uint32_t x = startX; x < worldB->data.width; ++x) {
                        Cell& cell = worldB->at(x, y);
                        // Only clear dirt cells, leave walls and other materials.
                        if (cell.material_type == MaterialType::DIRT && !cell.isWall()) {
                            cell.material_type = MaterialType::AIR;
                            cell.setFillRatio(0.0);
                            cell.setCOM(Vector2d{ 0.0, 0.0 });
                            cell.velocity = Vector2d{ 0.0, 0.0 };
                        }
                    }
                }
            }
        }

        spdlog::info("SimRunning: Toggle quadrant - now: {}", newValue);
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
