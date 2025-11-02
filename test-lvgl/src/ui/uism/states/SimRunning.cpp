#include "../StateMachine.h"
#include "State.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {
namespace State {

void SimRunning::onEnter(StateMachine& /*sm*/)
{
    spdlog::info("SimRunning: Simulation is running, displaying world updates");
    // TODO: Update UI to show running state.
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
    return SimRunning{ std::move(world) };
}

State::Any SimRunning::onEvent(const UiApi::MouseMove::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::debug("SimRunning: Mouse move at ({}, {})", cwc.command.pixelX, cwc.command.pixelY);

    // TODO: Handle mouse drag with running world.

    cwc.sendResponse(UiApi::MouseMove::Response::okay(std::monostate{}));
    return SimRunning{ std::move(world) };
}

State::Any SimRunning::onEvent(const UiApi::MouseUp::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::debug("SimRunning: Mouse up at ({}, {})", cwc.command.pixelX, cwc.command.pixelY);

    // TODO: Handle mouse release with running world.

    cwc.sendResponse(UiApi::MouseUp::Response::okay(std::monostate{}));
    return SimRunning{ std::move(world) };
}

State::Any SimRunning::onEvent(const UiApi::Screenshot::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::info("SimRunning: Screenshot command received");

    // TODO: Capture screenshot.

    std::string filepath = cwc.command.filepath.empty() ? "screenshot.png" : cwc.command.filepath;
    cwc.sendResponse(UiApi::Screenshot::Response::okay({filepath}));

    return SimRunning{ std::move(world) };
}

State::Any SimRunning::onEvent(const UiApi::SimPause::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::info("SimRunning: SimPause command received, pausing simulation");

    // TODO: Send pause command to DSSM server.

    cwc.sendResponse(UiApi::SimPause::Response::okay({true}));

    // Transition to Paused state.
    return Paused{ std::move(world) };
}

State::Any SimRunning::onEvent(const UiUpdateEvent& evt, StateMachine& /*sm*/)
{
    spdlog::trace("SimRunning: Received world update (step {})", evt.stepCount);

    // Update local world copy with received state.
    // TODO: Apply update to world and render.

    return SimRunning{ std::move(world) };
}

} // namespace State
} // namespace Ui
} // namespace DirtSim
