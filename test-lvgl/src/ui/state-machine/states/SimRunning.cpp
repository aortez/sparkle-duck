#include "State.h"
#include "ui/SimPlayground.h"
#include "ui/UiComponentManager.h"
#include "ui/state-machine/StateMachine.h"
#include "ui/state-machine/network/WebSocketClient.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {
namespace State {

void SimRunning::onEnter(StateMachine& sm)
{
    spdlog::info("SimRunning: Simulation is running, displaying world updates");

    // Create playground if not already created.
    if (!playground_) {
        playground_ = std::make_unique<SimPlayground>(
            sm.getUiComponentManager(), sm.getWebSocketClient(), sm);
        spdlog::info("SimRunning: Created simulation playground");
    }

    // Send initial frame_ready to kickstart the pipelined frame delivery.
    auto* wsClient = sm.getWebSocketClient();
    if (wsClient && wsClient->isConnected()) {
        nlohmann::json frameReadyCmd = { { "command", "frame_ready" } };
        wsClient->send(frameReadyCmd.dump());
        spdlog::info("SimRunning: Sent initial frame_ready to start frame delivery");
    }
}

void SimRunning::onExit(StateMachine& /*sm*/)
{
    spdlog::info("SimRunning: Exiting");
}

State::Any SimRunning::onEvent(const ServerDisconnectedEvent& evt, StateMachine& /*sm*/)
{
    spdlog::warn("SimRunning: Server disconnected (reason: {})", evt.reason);
    spdlog::info("SimRunning: Transitioning to Disconnected");

    // Lost connection - go back to Disconnected state (world is lost).
    return Disconnected{};
}

State::Any SimRunning::onEvent(const UiApi::DrawDebugToggle::Cwc& cwc, StateMachine& /*sm*/)
{
    using Response = UiApi::DrawDebugToggle::Response;

    debugDrawEnabled = cwc.command.enabled;
    spdlog::info("SimRunning: Debug draw mode {}", debugDrawEnabled ? "enabled" : "disabled");

    cwc.sendResponse(Response::okay(UiApi::DrawDebugToggle::Okay{ debugDrawEnabled }));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const UiApi::Exit::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::info("SimRunning: Exit command received, shutting down");

    // Send success response.
    cwc.sendResponse(UiApi::Exit::Response::okay(std::monostate{}));

    // Transition to Shutdown state.
    return Shutdown{};
}

State::Any SimRunning::onEvent(const UiApi::MouseDown::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::debug("SimRunning: Mouse down at ({}, {})", cwc.command.pixelX, cwc.command.pixelY);

    // TODO: Handle mouse interaction with running world.

    cwc.sendResponse(UiApi::MouseDown::Response::okay(std::monostate{}));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const UiApi::MouseMove::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::debug("SimRunning: Mouse move at ({}, {})", cwc.command.pixelX, cwc.command.pixelY);

    // TODO: Handle mouse drag with running world.

    cwc.sendResponse(UiApi::MouseMove::Response::okay(std::monostate{}));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const UiApi::MouseUp::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::debug("SimRunning: Mouse up at ({}, {})", cwc.command.pixelX, cwc.command.pixelY);

    // TODO: Handle mouse release with running world.

    cwc.sendResponse(UiApi::MouseUp::Response::okay(std::monostate{}));
    return std::move(*this);
}

State::Any SimRunning::onEvent(const UiApi::Screenshot::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::info("SimRunning: Screenshot command received");

    // TODO: Capture screenshot.

    std::string filepath = cwc.command.filepath.empty() ? "screenshot.png" : cwc.command.filepath;
    cwc.sendResponse(UiApi::Screenshot::Response::okay({ filepath }));

    return std::move(*this);
}

State::Any SimRunning::onEvent(const UiApi::SimPause::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::info("SimRunning: SimPause command received, pausing simulation");

    // TODO: Send pause command to DSSM server.

    cwc.sendResponse(UiApi::SimPause::Response::okay({ true }));

    // Transition to Paused state (keep renderer for when we resume).
    return Paused{ std::move(worldData) };
}

State::Any SimRunning::onEvent(const FrameReadyNotification& evt, StateMachine& sm)
{
    // Time-based frame limiting: only request updates at target frame rate.
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrameTime);

    const std::chrono::milliseconds targetFrameInterval{ 16 }; // 60 FPS target.
    if (elapsed >= targetFrameInterval) {
        // Enough time passed - request new frame.
        spdlog::info(
            "SimRunning: Frame ready (step {}), requesting update (skipped {} frames)",
            evt.stepNumber,
            skippedFrames);

        // Calculate actual UI FPS.
        if (elapsed.count() > 0) {
            measuredUiFps = 1000.0 / elapsed.count();

            // Exponentially weighted moving average (90% old, 10% new) for smooth display.
            if (smoothedUiFps == 0.0) {
                smoothedUiFps = measuredUiFps; // Initialize.
            }
            else {
                smoothedUiFps = 0.9 * smoothedUiFps + 0.1 * measuredUiFps;
            }

            spdlog::info(
                "SimRunning: UI FPS: {:.1f} (smoothed: {:.1f})", measuredUiFps, smoothedUiFps);
        }

        // Request world state from DSSM (only if no request is pending).
        auto* wsClient = sm.getWebSocketClient();
        if (wsClient && wsClient->isConnected() && !stateGetPending) {
            nlohmann::json stateGetCmd = { { "command", "state_get" } };
            wsClient->send(stateGetCmd.dump());

            // Record when request was sent for round-trip timing.
            lastStateGetSentTime = std::chrono::steady_clock::now();
            stateGetPending = true;
            spdlog::debug("SimRunning: Sent state_get request (step {})", evt.stepNumber);
        }
        else if (stateGetPending) {
            spdlog::debug(
                "SimRunning: Skipping state_get request - previous request still pending (step {})",
                evt.stepNumber);
        }

        lastFrameTime = now;
        skippedFrames = 0;
    }
    else {
        // Too soon - skip this frame.
        skippedFrames++;
        spdlog::debug(
            "SimRunning: Skipping frame {} (elapsed {}ms < target {}ms)",
            evt.stepNumber,
            elapsed.count(),
            targetFrameInterval.count());
    }

    return std::move(*this);
}

State::Any SimRunning::onEvent(const UiUpdateEvent& evt, StateMachine& sm)
{
    spdlog::debug("SimRunning: Received world update (step {}) via push", evt.stepCount);

    // Send frame_ready IMMEDIATELY to pipeline next frame (hide network latency).
    auto* wsClient = sm.getWebSocketClient();
    if (wsClient && wsClient->isConnected()) {
        nlohmann::json frameReadyCmd = { { "command", "frame_ready" } };
        wsClient->send(frameReadyCmd.dump());
        spdlog::trace("SimRunning: Sent frame_ready to server (pipelining next frame)");
    }

    // Calculate UI FPS based on time between updates.
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrameTime);

    if (elapsed.count() > 0) {
        measuredUiFps = 1000.0 / elapsed.count();

        // Exponentially weighted moving average (90% old, 10% new) for smooth display.
        if (smoothedUiFps == 0.0) {
            smoothedUiFps = measuredUiFps; // Initialize.
        }
        else {
            smoothedUiFps = 0.9 * smoothedUiFps + 0.1 * measuredUiFps;
        }

        spdlog::debug(
            "SimRunning: UI FPS: {:.1f} (smoothed: {:.1f})", measuredUiFps, smoothedUiFps);
    }

    lastFrameTime = now;

    // Log performance stats every 20 updates.
    updateCount++;
    if (updateCount % 20 == 0) {
        auto& timers = sm.getTimers();

        spdlog::info("UI Performance Stats (after {} updates):", updateCount);
        spdlog::info(
            "  Message parse: {:.1f}ms avg ({} calls, {:.1f}ms total)",
            timers.getCallCount("parse_message") > 0
                ? timers.getAccumulatedTime("parse_message") / timers.getCallCount("parse_message")
                : 0.0,
            timers.getCallCount("parse_message"),
            timers.getAccumulatedTime("parse_message"));
        spdlog::info(
            "  World render: {:.1f}ms avg ({} calls, {:.1f}ms total)",
            timers.getCallCount("render_world") > 0
                ? timers.getAccumulatedTime("render_world") / timers.getCallCount("render_world")
                : 0.0,
            timers.getCallCount("render_world"),
            timers.getAccumulatedTime("render_world"));
    }

    // Update local worldData with received state.
    worldData = std::make_unique<WorldData>(evt.worldData);

    // Update and render via playground.
    if (playground_ && worldData) {
        // Update controls with new world state.
        playground_->updateFromWorldData(*worldData, smoothedUiFps);

        // Render world.
        sm.getTimers().startTimer("render_world");
        playground_->render(*worldData, debugDrawEnabled);
        sm.getTimers().stopTimer("render_world");

        spdlog::debug(
            "SimRunning: Rendered world ({}x{}, step {})",
            worldData->width,
            worldData->height,
            worldData->timestep);
    }

    return std::move(*this);
}

} // namespace State
} // namespace Ui
} // namespace DirtSim
