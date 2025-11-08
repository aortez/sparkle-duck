#include "ui/state-machine/StateMachine.h"
#include "State.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {
namespace State {

void Paused::onEnter(StateMachine& /*sm*/)
{
    spdlog::info("Paused: Simulation paused, displaying frozen world state");
    // TODO: Update UI to show paused state.
}

void Paused::onExit(StateMachine& /*sm*/)
{
    spdlog::info("Paused: Exiting");
}

State::Any Paused::onEvent(const UiApi::Exit::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::info("Paused: Exit command received, shutting down");

    // Send success response.
    cwc.sendResponse(UiApi::Exit::Response::okay(std::monostate{}));

    // Transition to Shutdown state.
    return Shutdown{};
}

State::Any Paused::onEvent(const UiApi::MouseDown::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::debug("Paused: Mouse down at ({}, {})", cwc.command.pixelX, cwc.command.pixelY);

    // TODO: Handle mouse interaction with paused world.

    cwc.sendResponse(UiApi::MouseDown::Response::okay(std::monostate{}));
    return Paused{ std::move(worldData) };
}

State::Any Paused::onEvent(const UiApi::MouseMove::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::debug("Paused: Mouse move at ({}, {})", cwc.command.pixelX, cwc.command.pixelY);

    // TODO: Handle mouse drag with paused world.

    cwc.sendResponse(UiApi::MouseMove::Response::okay(std::monostate{}));
    return Paused{ std::move(worldData) };
}

State::Any Paused::onEvent(const UiApi::MouseUp::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::debug("Paused: Mouse up at ({}, {})", cwc.command.pixelX, cwc.command.pixelY);

    // TODO: Handle mouse release with paused world.

    cwc.sendResponse(UiApi::MouseUp::Response::okay(std::monostate{}));
    return Paused{ std::move(worldData) };
}

State::Any Paused::onEvent(const UiApi::Screenshot::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::info("Paused: Screenshot command received");

    // TODO: Capture screenshot.

    std::string filepath = cwc.command.filepath.empty() ? "screenshot.png" : cwc.command.filepath;
    cwc.sendResponse(UiApi::Screenshot::Response::okay({filepath}));

    return Paused{ std::move(worldData) };
}

State::Any Paused::onEvent(const UiApi::SimRun::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::info("Paused: SimRun command received, resuming simulation");

    // TODO: Send resume command to DSSM server.

    cwc.sendResponse(UiApi::SimRun::Response::okay({true}));

    // Transition back to SimRunning (renderer and controls will be created in onEnter).
    SimRunning newState;
    newState.worldData = std::move(worldData);
    return newState;
}

State::Any Paused::onEvent(const ServerDisconnectedEvent& evt, StateMachine& /*sm*/)
{
    spdlog::warn("Paused: Server disconnected (reason: {})", evt.reason);
    spdlog::info("Paused: Transitioning to Disconnected");

    // Lost connection - go back to Disconnected state (world is lost).
    return Disconnected{};
}

} // namespace State
} // namespace Ui
} // namespace DirtSim
