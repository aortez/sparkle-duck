#include "../StateMachine.h"
#include "../network/WebSocketClient.h"
#include "State.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {
namespace State {

void StartMenu::onEnter(StateMachine& /*sm*/)
{
    spdlog::info("StartMenu: Connected to server, ready to start simulation");
    spdlog::info("StartMenu: Show simulation controls (start, scenario selection, etc.)");
    // TODO: Display start menu UI using SimulatorUI.
}

void StartMenu::onExit(StateMachine& /*sm*/)
{
    spdlog::info("StartMenu: Exiting");
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
