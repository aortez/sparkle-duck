#include "../StateMachine.h"
#include "../network/WebSocketClient.h"
#include "../../UiComponentManager.h"
#include "../api/SimRun.h"
#include "State.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {
namespace State {

void StartMenu::onEnter(StateMachine& sm)
{
    spdlog::info("StartMenu: Connected to server, ready to start simulation");

    // Get main menu container (switches to menu screen).
    auto* uiManager = sm.getUiComponentManager();
    if (!uiManager) return;

    lv_obj_t* container = uiManager->getMainMenuContainer();

    // Create centered "Start Simulation" button.
    lv_obj_t* startButton = lv_btn_create(container);
    lv_obj_set_size(startButton, 200, 60);
    lv_obj_center(startButton);
    lv_obj_set_user_data(startButton, &sm);
    lv_obj_add_event_cb(startButton, onStartButtonClicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* label = lv_label_create(startButton);
    lv_label_set_text(label, "Start Simulation");
    lv_obj_center(label);

    spdlog::info("StartMenu: Created start button");
}

void StartMenu::onExit(StateMachine& /*sm*/)
{
    spdlog::info("StartMenu: Exiting");
    // No cleanup needed - screen switch will clean up widgets automatically.
}

void StartMenu::onStartButtonClicked(lv_event_t* e)
{
    auto* sm = static_cast<StateMachine*>(lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e))));
    if (!sm) return;

    spdlog::info("StartMenu: Start button clicked, sending sim_run to DSSM");

    // Send sim_run command to DSSM using typed Command object.
    auto* wsClient = sm->getWebSocketClient();
    if (wsClient && wsClient->isConnected()) {
        UiApi::SimRun::Command cmd;  // Create command object.
        nlohmann::json json = cmd.toJson();  // Convert to JSON.
        wsClient->send(json.dump());  // Send to DSSM.
    }
    else {
        spdlog::error("StartMenu: Cannot start simulation, not connected to DSSM");
    }
}

State::Any StartMenu::onEvent(const FrameReadyNotification& evt, StateMachine& /*sm*/)
{
    spdlog::info("StartMenu: Received frame_ready (step {}), server already running simulation", evt.stepNumber);
    spdlog::info("StartMenu: Transitioning to SimRunning to display visualization");

    // Server already has a running simulation - transition to SimRunning to render it.
    return SimRunning{ nullptr, nullptr, nullptr };  // Will initialize worldData, renderer, and controls in onEnter.
}

State::Any StartMenu::onEvent(const ServerDisconnectedEvent& evt, StateMachine& /*sm*/)
{
    spdlog::warn("StartMenu: Server disconnected (reason: {})", evt.reason);
    spdlog::info("StartMenu: Transitioning back to Disconnected");

    // Lost connection - go back to Disconnected state.
    return Disconnected{};
}

State::Any StartMenu::onEvent(const UiApi::Exit::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::info("StartMenu: Exit command received, shutting down");

    // Send success response.
    cwc.sendResponse(UiApi::Exit::Response::okay(std::monostate{}));

    // Transition to Shutdown state.
    return Shutdown{};
}

State::Any StartMenu::onEvent(const UiApi::SimRun::Cwc& cwc, StateMachine& sm)
{
    spdlog::info("StartMenu: SimRun command received");

    // Get WebSocket client to send command to DSSM.
    auto* wsClient = sm.getWebSocketClient();
    if (!wsClient || !wsClient->isConnected()) {
        spdlog::error("StartMenu: Not connected to DSSM server");
        cwc.sendResponse(UiApi::SimRun::Response::error(ApiError("Not connected to DSSM server")));
        return StartMenu{};
    }

    // Send sim_run command to DSSM server.
    nlohmann::json simRunCmd = {{"command", "sim_run"}};
    bool sent = wsClient->send(simRunCmd.dump());

    if (!sent) {
        spdlog::error("StartMenu: Failed to send sim_run to DSSM");
        cwc.sendResponse(UiApi::SimRun::Response::error(ApiError("Failed to send command to DSSM")));
        return StartMenu{};
    }

    spdlog::info("StartMenu: Sent sim_run to DSSM, transitioning to SimRunning");

    // Send OK response.
    cwc.sendResponse(UiApi::SimRun::Response::okay({true}));

    // Transition to SimRunning state.
    return SimRunning{};
}

} // namespace State
} // namespace Ui
} // namespace DirtSim
