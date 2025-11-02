#include "../StateMachine.h"
#include "State.h"
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

State::Any StartMenu::onEvent(const UiApi::SimRun::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::info("StartMenu: SimRun command received");

    // TODO: Send SimRun command to DSSM server via WebSocket client.
    // TODO: Transition to SimRunning state when server confirms.

    spdlog::warn("StartMenu: Simulation control not yet implemented - staying in menu");

    // Send OK response for now.
    cwc.sendResponse(UiApi::SimRun::Response::okay({true}));

    return StartMenu{};
}

} // namespace State
} // namespace Ui
} // namespace DirtSim
