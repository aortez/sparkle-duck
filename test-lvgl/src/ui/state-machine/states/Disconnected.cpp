#include "../StateMachine.h"
#include "State.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {
namespace State {

void Disconnected::onEnter(StateMachine& /*sm*/)
{
    spdlog::info("Disconnected: Not connected to DSSM server");
    spdlog::info("Disconnected: Show connection UI (host/port input, connect button)");
    // TODO: Display connection UI using SimulatorUI.
}

void Disconnected::onExit(StateMachine& /*sm*/)
{
    spdlog::info("Disconnected: Exiting");
}

State::Any Disconnected::onEvent(const ConnectToServerCommand& cmd, StateMachine& /*sm*/)
{
    spdlog::info("Disconnected: Connect command received (host={}, port={})", cmd.host, cmd.port);

    // TODO: Initiate WebSocket connection to server.
    // TODO: On success, queue ServerConnectedEvent.
    // For now, just log and stay in same state.

    spdlog::warn("Disconnected: WebSocket client not yet implemented - staying disconnected");

    return Disconnected{};
}

State::Any Disconnected::onEvent(const ServerConnectedEvent& /*evt*/, StateMachine& /*sm*/)
{
    spdlog::info("Disconnected: Server connection established");
    spdlog::info("Disconnected: Transitioning to StartMenu");

    // Transition to StartMenu state (show simulation controls).
    return StartMenu{};
}

State::Any Disconnected::onEvent(const UiApi::Exit::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::info("Disconnected: Exit command received, shutting down");

    // Send success response.
    cwc.sendResponse(UiApi::Exit::Response::okay(std::monostate{}));

    // Transition to Shutdown state.
    return Shutdown{};
}

} // namespace State
} // namespace Ui
} // namespace DirtSim
