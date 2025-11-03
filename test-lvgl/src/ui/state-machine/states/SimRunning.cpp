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
        controls_ = std::make_unique<ControlPanel>(container, sm.getWebSocketClient());
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
    return SimRunning{ std::move(worldData), std::move(renderer_), std::move(controls_) };
}

State::Any SimRunning::onEvent(const UiApi::MouseMove::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::debug("SimRunning: Mouse move at ({}, {})", cwc.command.pixelX, cwc.command.pixelY);

    // TODO: Handle mouse drag with running world.

    cwc.sendResponse(UiApi::MouseMove::Response::okay(std::monostate{}));
    return SimRunning{ std::move(worldData), std::move(renderer_), std::move(controls_) };
}

State::Any SimRunning::onEvent(const UiApi::MouseUp::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::debug("SimRunning: Mouse up at ({}, {})", cwc.command.pixelX, cwc.command.pixelY);

    // TODO: Handle mouse release with running world.

    cwc.sendResponse(UiApi::MouseUp::Response::okay(std::monostate{}));
    return SimRunning{ std::move(worldData), std::move(renderer_), std::move(controls_) };
}

State::Any SimRunning::onEvent(const UiApi::Screenshot::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::info("SimRunning: Screenshot command received");

    // TODO: Capture screenshot.

    std::string filepath = cwc.command.filepath.empty() ? "screenshot.png" : cwc.command.filepath;
    cwc.sendResponse(UiApi::Screenshot::Response::okay({filepath}));

    return SimRunning{ std::move(worldData), std::move(renderer_), std::move(controls_) };
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
    spdlog::info("SimRunning: Frame ready notification (step {})", evt.stepNumber);

    // Request world state from DSSM.
    auto* wsClient = sm.getWebSocketClient();
    if (wsClient && wsClient->isConnected()) {
        nlohmann::json stateGetCmd = {{"command", "state_get"}};
        wsClient->send(stateGetCmd.dump());
        spdlog::debug("SimRunning: Requested state_get from DSSM");
    }

    return SimRunning{ std::move(worldData), std::move(renderer_), std::move(controls_) };
}

State::Any SimRunning::onEvent(const UiUpdateEvent& evt, StateMachine& sm)
{
    spdlog::trace("SimRunning: Received world update (step {})", evt.stepCount);

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
            bool debugDraw = worldData->debug_draw_enabled;
            renderer_->renderWorldData(*worldData, container, debugDraw);
            spdlog::debug("SimRunning: Rendered world ({}x{}, step {})",
                         worldData->width, worldData->height, worldData->timestep);
        }
    }

    return SimRunning{ std::move(worldData), std::move(renderer_), std::move(controls_) };
}

} // namespace State
} // namespace Ui
} // namespace DirtSim
