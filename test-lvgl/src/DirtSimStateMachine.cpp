#include "DirtSimStateMachine.h"
#include "EventDispatcher.h"
#include "WorldSetup.h"
#include <chrono>
#include <spdlog/spdlog.h>
#include <thread>

namespace DirtSim {

DirtSimStateMachine::DirtSimStateMachine(lv_disp_t* display)
    : display(display),
      eventProcessor(),
      sharedState(),
      eventRouter(std::make_unique<EventRouter>(*this, sharedState, eventProcessor.getEventQueue()))
{

    // Initialize UIManager if display is available
    if (display) {
        uiManager = std::make_unique<UIManager>(display);
        spdlog::info(
            "DirtSimStateMachine initialized with UI support in state: {}", getCurrentStateName());
    }
    else {
        spdlog::info(
            "DirtSimStateMachine initialized in headless mode in state: {}", getCurrentStateName());
    }
}

DirtSimStateMachine::~DirtSimStateMachine()
{
    spdlog::info("DirtSimStateMachine shutting down from state: {}", getCurrentStateName());
}

void DirtSimStateMachine::mainLoopRun()
{
    spdlog::info("Starting main event loop");

    // Initialize by sending init complete event
    queueEvent(InitCompleteEvent{});

    // Main event processing loop
    while (!shouldExit()) {
        // Process events from queue
        eventProcessor.processEventsFromQueue(*this);

        // If we're in SimRunning state and not paused, advance simulation
        if (std::holds_alternative<State::SimRunning>(fsmState) && !sharedState.getIsPaused()) {
            queueEvent(AdvanceSimulationCommand{});
        }

        // Small sleep to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }

    spdlog::info("Main event loop exiting");
}

void DirtSimStateMachine::queueEvent(const Event& event)
{
    eventProcessor.queueEvent(event);
}

void DirtSimStateMachine::processImmediateEvent(const Event& event, SharedSimState& shared)
{
    // Immediate events are processed directly without state dispatch
    std::visit(
        [this, &shared](const auto& e) {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, GetFPSCommand>) {
                // Already handled by EventRouter
            }
            else if constexpr (std::is_same_v<T, GetSimStatsCommand>) {
                // Already handled by EventRouter
            }
            else if constexpr (std::is_same_v<T, PauseCommand>) {
                // Already handled by EventRouter
            }
            else if constexpr (std::is_same_v<T, ResumeCommand>) {
                // Already handled by EventRouter
            }
        },
        event);
}

void DirtSimStateMachine::handleEvent(const Event& event)
{
    // Save the current state index before moving
    std::size_t oldStateIndex = fsmState.index();

    // Use EventDispatcher to route to current state
    // Move the current state to dispatch
    State::Any newState = EventDispatcher::dispatch(std::move(fsmState), event, *this);

    // Check if the state type changed
    if (newState.index() != oldStateIndex) {
        // State type changed - use transitionTo to handle lifecycle
        transitionTo(std::move(newState));
    }
    else {
        // Same state type - just update without lifecycle calls
        fsmState = std::move(newState);
    }
}

void DirtSimStateMachine::transitionTo(State::Any newState)
{
    std::string oldStateName = getCurrentStateName();

    // Call onExit for current state
    std::visit([this](auto& state) { callOnExit(state); }, fsmState);

    // Perform transition
    fsmState = std::move(newState);

    std::string newStateName = getCurrentStateName();
    spdlog::info("STATE_TRANSITION: {} -> {}", oldStateName, newStateName);

    // Call onEnter for new state
    std::visit([this](auto& state) { callOnEnter(state); }, fsmState);
}

// Global event handlers

State::Any DirtSimStateMachine::onEvent(const QuitApplicationCommand& /*cmd*/)
{
    spdlog::info("Global handler: QuitApplicationCommand received");
    sharedState.setShouldExit(true);
    return State::Shutdown{};
}

State::Any DirtSimStateMachine::onEvent(const GetFPSCommand& /*cmd*/)
{
    // This is an immediate event, should not reach here
    spdlog::warn("GetFPSCommand reached global handler - should be immediate");
    // Return a default-constructed state of the same type
    return std::visit(
        [](auto&& state) -> State::Any {
            using T = std::decay_t<decltype(state)>;
            return T{};
        },
        fsmState);
}

State::Any DirtSimStateMachine::onEvent(const GetSimStatsCommand& /*cmd*/)
{
    // This is an immediate event, should not reach here
    spdlog::warn("GetSimStatsCommand reached global handler - should be immediate");
    // Return a default-constructed state of the same type
    return std::visit(
        [](auto&& state) -> State::Any {
            using T = std::decay_t<decltype(state)>;
            return T{};
        },
        fsmState);
}

} // namespace DirtSim