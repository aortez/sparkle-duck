#include "../core/World.h"  // Must be first for complete type in variant.
#include "../core/WorldData.h"
#include "../core/WorldEventGenerator.h"
#include "StateMachine.h"
#include "scenarios/Scenario.h"
#include "scenarios/ScenarioRegistry.h"
#include <cassert>
#include <chrono>
#include <spdlog/spdlog.h>
#include <thread>

namespace DirtSim {
namespace Server {

StateMachine::StateMachine() : eventProcessor()
{
    spdlog::info("Server::StateMachine initialized in headless mode in state: {}", getCurrentStateName());
    // Note: World will be created by SimRunning state when simulation starts.
}

StateMachine::~StateMachine()
{
    spdlog::info("Server::StateMachine shutting down from state: {}", getCurrentStateName());
}

void StateMachine::updateCachedWorldData(const WorldData& data)
{
    std::lock_guard<std::mutex> lock(cachedWorldDataMutex_);
    cachedWorldData_ = std::make_shared<WorldData>(data);
}

std::shared_ptr<WorldData> StateMachine::getCachedWorldData() const
{
    std::lock_guard<std::mutex> lock(cachedWorldDataMutex_);
    return cachedWorldData_;  // Returns shared_ptr (may be nullptr).
}


void StateMachine::mainLoopRun()
{
    spdlog::info("Starting main event loop");

    // Initialize by sending init complete event.
    queueEvent(InitCompleteEvent{});

    // Main event processing loop.
    while (!shouldExit()) {
        // Process events from queue.
        eventProcessor.processEventsFromQueue(*this);

        // Queue simulation advance commands only when actively running.
        // When in SimRunning state, the simulation should advance.
        // When in SimPaused state, no automatic advancing (but manual stepping is allowed).
        if (std::holds_alternative<State::SimRunning>(fsmState)) {
            queueEvent(AdvanceSimulationCommand{});
        }

        // Small sleep to prevent busy waiting (1ms for max performance testing).
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    spdlog::info("State machine event loop exiting (shouldExit=true)");

    spdlog::info("Main event loop exiting");
}

void StateMachine::queueEvent(const Event& event)
{
    eventProcessor.enqueueEvent(event);
}

void StateMachine::processEvents()
{
    eventProcessor.processEventsFromQueue(*this);
}

void StateMachine::handleEvent(const Event& event)
{
    spdlog::debug("Server::StateMachine: Handling event: {}", getEventName(event));

    std::visit(
        [this](auto&& evt) {
            std::visit(
                [this, &evt](auto&& state) -> void {
                    using StateType = std::decay_t<decltype(state)>;

                    if constexpr (requires { state.onEvent(evt, *this); }) {
                        auto newState = state.onEvent(evt, *this);
                        if (!std::holds_alternative<StateType>(newState)) {
                            transitionTo(std::move(newState));
                        }
                        else {
                            // Same state type - move it back into variant to preserve state.
                            fsmState = std::move(newState);
                        }
                    }
                    else {
                        spdlog::warn(
                            "Server::StateMachine: State {} does not handle event {}",
                            State::getCurrentStateName(fsmState),
                            getEventName(Event{ evt }));

                        // If this is an API command with sendResponse, send error.
                        if constexpr (requires { evt.sendResponse(std::declval<typename std::decay_t<decltype(evt)>::Response>()); }) {
                            auto errorMsg = std::string("Command not supported in state: ") + State::getCurrentStateName(fsmState);
                            using EventType = std::decay_t<decltype(evt)>;
                            using ResponseType = typename EventType::Response;
                            evt.sendResponse(ResponseType::error(ApiError(errorMsg)));
                        }
                    }
                },
                fsmState);
        },
        event);
}

void StateMachine::transitionTo(State::Any newState)
{
    std::string oldStateName = getCurrentStateName();

    // Call onExit for current state.
    std::visit([this](auto& state) { callOnExit(state); }, fsmState);

    // Perform transition.
    fsmState = std::move(newState);

    std::string newStateName = getCurrentStateName();
    spdlog::info("STATE_TRANSITION: {} -> {}", oldStateName, newStateName);

    // Call onEnter for new state.
    std::visit([this](auto& state) { callOnEnter(state); }, fsmState);
}

// Global event handlers.

State::Any StateMachine::onEvent(const QuitApplicationCommand& /*cmd.*/)
{
    spdlog::info("Global handler: QuitApplicationCommand received");
    setShouldExit(true);
    return State::Shutdown{};
}

State::Any StateMachine::onEvent(const GetFPSCommand& /*cmd.*/)
{
    // This is an immediate event, should not reach here.
    spdlog::warn("GetFPSCommand reached global handler - should be immediate");
    // Return a default-constructed state of the same type.
    return std::visit(
        [](auto&& state) -> State::Any {
            using T = std::decay_t<decltype(state)>;
            return T{};
        },
        fsmState);
}

State::Any StateMachine::onEvent(const GetSimStatsCommand& /*cmd.*/)
{
    // This is an immediate event, should not reach here.
    spdlog::warn("GetSimStatsCommand reached global handler - should be immediate");
    // Return a default-constructed state of the same type.
    return std::visit(
        [](auto&& state) -> State::Any {
            using T = std::decay_t<decltype(state)>;
            return T{};
        },
        fsmState);
}


} // namespace Server
} // namespace DirtSim
