#include "../StateMachine.h"
#include "../network/WebSocketClient.h"
#include "../../rendering/CellRenderer.h"
#include "../../UiComponentManager.h"
#include "State.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {
namespace State {

void SimRunning::onEnter(StateMachine& sm)
{
    spdlog::info("SimRunning: Simulation is running, displaying world updates");

    // Get simulation container from UiComponentManager.
    auto* uiManager = sm.getUiComponentManager();
    if (!uiManager) return;

    lv_obj_t* container = uiManager->getSimulationContainer();

    // Create renderer if not already created.
    if (!renderer_) {
        renderer_ = std::make_unique<CellRenderer>();
    }

    // Create control panel if not already created.
    if (!controls_) {
        controls_ = std::make_unique<ControlPanel>(container, sm.getWebSocketClient(), sm);
        spdlog::info("SimRunning: Created control panel");
    }

    spdlog::info("SimRunning: Got simulation container for rendering");
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

    cwc.sendResponse(Response::okay(UiApi::DrawDebugToggle::Okay{debugDrawEnabled}));
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
    cwc.sendResponse(UiApi::Screenshot::Response::okay({filepath}));

    return std::move(*this);
}

State::Any SimRunning::onEvent(const UiApi::SimPause::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::info("SimRunning: SimPause command received, pausing simulation");

    // TODO: Send pause command to DSSM server.

    cwc.sendResponse(UiApi::SimPause::Response::okay({true}));

    // Transition to Paused state (keep renderer for when we resume).
    return Paused{ std::move(worldData) };
}

State::Any SimRunning::onEvent(const FrameReadyNotification& evt, StateMachine& sm)
{
    // Time-based frame limiting: only request updates at target frame rate.
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrameRequestTime);

    if (elapsed >= targetFrameInterval) {
        // Enough time passed - request new frame.
        spdlog::info("SimRunning: Frame ready (step {}), requesting update (skipped {} frames)",
                     evt.stepNumber, skippedFrames);

        // Calculate actual UI FPS.
        if (elapsed.count() > 0) {
            measuredUIFPS = 1000.0 / elapsed.count();

            // Exponentially weighted moving average (90% old, 10% new) for smooth display.
            if (smoothedUIFPS == 0.0) {
                smoothedUIFPS = measuredUIFPS;  // Initialize.
            } else {
                smoothedUIFPS = 0.9 * smoothedUIFPS + 0.1 * measuredUIFPS;
            }

            spdlog::info("SimRunning: UI FPS: {:.1f} (smoothed: {:.1f})",
                         measuredUIFPS, smoothedUIFPS);
        }

        // Request world state from DSSM.
        auto* wsClient = sm.getWebSocketClient();
        if (wsClient && wsClient->isConnected()) {
            nlohmann::json stateGetCmd = {{"command", "state_get"}};
            wsClient->send(stateGetCmd.dump());

            // Record when request was sent for round-trip timing.
            lastStateGetSentTime = std::chrono::steady_clock::now();
        }

        lastFrameRequestTime = now;
        skippedFrames = 0;
    } else {
        // Too soon - skip this frame.
        skippedFrames++;
        spdlog::debug("SimRunning: Skipping frame {} (elapsed {}ms < target {}ms)",
                      evt.stepNumber, elapsed.count(), targetFrameInterval.count());
    }

    return std::move(*this);
}

State::Any SimRunning::onEvent(const UiUpdateEvent& evt, StateMachine& sm)
{
    // Calculate round-trip time for frame request.
    auto now = std::chrono::steady_clock::now();
    auto roundTrip = std::chrono::duration_cast<std::chrono::microseconds>(now - lastStateGetSentTime);
    lastRoundTripMs = roundTrip.count() / 1000.0;  // Convert to milliseconds.

    // Smooth with EMA.
    if (smoothedRoundTripMs == 0.0) {
        smoothedRoundTripMs = lastRoundTripMs;
    } else {
        smoothedRoundTripMs = 0.9 * smoothedRoundTripMs + 0.1 * lastRoundTripMs;
    }

    spdlog::info("SimRunning: Received world update (step {}), round-trip: {:.1f}ms (smoothed: {:.1f}ms)",
                 evt.stepCount, lastRoundTripMs, smoothedRoundTripMs);

    // Update local worldData with received state.
    worldData = std::make_unique<WorldData>(evt.worldData);

    // Update control panel with new world state.
    if (controls_ && worldData) {
        controls_->updateFromWorldData(*worldData);
    }

    // Render worldData to LVGL.
    if (renderer_ && worldData) {
        auto* uiManager = sm.getUiComponentManager();
        if (uiManager) {
            lv_obj_t* container = uiManager->getSimulationContainer();
            renderer_->renderWorldData(*worldData, container, debugDrawEnabled);
            spdlog::debug("SimRunning: Rendered world ({}x{}, step {})",
                         worldData->width, worldData->height, worldData->timestep);
        }
    }

    return std::move(*this);
}

} // namespace State
} // namespace Ui
} // namespace DirtSim
