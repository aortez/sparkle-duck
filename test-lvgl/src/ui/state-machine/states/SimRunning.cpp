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

State::Any SimRunning::onEvent(const UiApi::PixelRendererToggle::Cwc& cwc, StateMachine& /*sm*/)
{
    using Response = UiApi::PixelRendererToggle::Response;

    pixelRendererEnabled = cwc.command.enabled;
    spdlog::info("SimRunning: Pixel renderer mode {}", pixelRendererEnabled ? "enabled" : "disabled");

    cwc.sendResponse(Response::okay(UiApi::PixelRendererToggle::Okay{ pixelRendererEnabled }));
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

    updateCount++;
    // Log performance stats every once in a while.
    if (updateCount % 100 == 0) {
        auto& timers = sm.getTimers();

        // Get current stats
        double parseTotal = timers.getAccumulatedTime("parse_message");
        uint32_t parseCount = timers.getCallCount("parse_message");
        double renderTotal = timers.getAccumulatedTime("render_world");
        uint32_t renderCount = timers.getCallCount("render_world");

        // Calculate interval stats (last 20 updates)
        static double lastParseTotal = 0.0;
        static uint32_t lastParseCount = 0;
        static double lastRenderTotal = 0.0;
        static uint32_t lastRenderCount = 0;

        double intervalParseTime = parseTotal - lastParseTotal;
        uint32_t intervalParseCount = parseCount - lastParseCount;
        double intervalRenderTime = renderTotal - lastRenderTotal;
        uint32_t intervalRenderCount = renderCount - lastRenderCount;

        // Get additional timing info
        double copyTotal = timers.getAccumulatedTime("copy_worlddata");
        uint32_t copyCount = timers.getCallCount("copy_worlddata");
        double updateTotal = timers.getAccumulatedTime("update_controls");
        uint32_t updateCount_ = timers.getCallCount("update_controls");

        static double lastCopyTotal = 0.0;
        static uint32_t lastCopyCount = 0;
        static double lastUpdateTotal = 0.0;
        static uint32_t lastUpdateCount = 0;

        double intervalCopyTime = copyTotal - lastCopyTotal;
        uint32_t intervalCopyCount = copyCount - lastCopyCount;
        double intervalUpdateTime = updateTotal - lastUpdateTotal;
        uint32_t intervalUpdateCount = updateCount_ - lastUpdateCount;

        spdlog::info("UI Performance Stats (last 20 updates, total {}):", updateCount);
        spdlog::info(
            "  Message parse: {:.1f}ms avg ({} calls, {:.1f}ms interval)",
            intervalParseCount > 0 ? intervalParseTime / intervalParseCount : 0.0,
            intervalParseCount,
            intervalParseTime);
        spdlog::info(
            "  WorldData copy: {:.1f}ms avg ({} calls, {:.1f}ms interval)",
            intervalCopyCount > 0 ? intervalCopyTime / intervalCopyCount : 0.0,
            intervalCopyCount,
            intervalCopyTime);
        spdlog::info(
            "  Update controls: {:.1f}ms avg ({} calls, {:.1f}ms interval)",
            intervalUpdateCount > 0 ? intervalUpdateTime / intervalUpdateCount : 0.0,
            intervalUpdateCount,
            intervalUpdateTime);
        spdlog::info(
            "  World render: {:.1f}ms avg ({} calls, {:.1f}ms interval)",
            intervalRenderCount > 0 ? intervalRenderTime / intervalRenderCount : 0.0,
            intervalRenderCount,
            intervalRenderTime);

        lastCopyTotal = copyTotal;
        lastCopyCount = copyCount;
        lastUpdateTotal = updateTotal;
        lastUpdateCount = updateCount_;

        // Store current totals for next interval
        lastParseTotal = parseTotal;
        lastParseCount = parseCount;
        lastRenderTotal = renderTotal;
        lastRenderCount = renderCount;
    }

    // Update local worldData with received state.
    sm.getTimers().startTimer("copy_worlddata");
    worldData = std::make_unique<WorldData>(evt.worldData);
    sm.getTimers().stopTimer("copy_worlddata");

    // Update and render via playground.
    if (playground_ && worldData) {
        // Update controls with new world state.
        sm.getTimers().startTimer("update_controls");
        playground_->updateFromWorldData(*worldData, smoothedUiFps);
        sm.getTimers().stopTimer("update_controls");

        // Render world.
        sm.getTimers().startTimer("render_world");
        playground_->render(*worldData, debugDrawEnabled, pixelRendererEnabled);
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
