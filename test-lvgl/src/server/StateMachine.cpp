#include "StateMachine.h"
#include "core/ScenarioConfig.h"
#include "core/World.h" // Must be first for complete type in variant.
#include "core/WorldData.h"
#include "core/WorldEventGenerator.h"
#include "scenarios/Scenario.h"
#include "scenarios/ScenarioRegistry.h"
#include <cassert>
#include <chrono>
#include <spdlog/spdlog.h>
#include <thread>

namespace DirtSim {
namespace Server {

StateMachine::StateMachine()
    : eventProcessor(), scenarioRegistry_(ScenarioRegistry::createDefault())
{
    spdlog::info(
        "Server::StateMachine initialized in headless mode in state: {}", getCurrentStateName());
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
    return cachedWorldData_; // Returns shared_ptr (may be nullptr).
}

void StateMachine::mainLoopRun()
{
    spdlog::info("Starting main event loop");

    // Initialize by sending init complete event.
    queueEvent(InitCompleteEvent{});

    // Main event processing loop.
    while (!shouldExit()) {
        auto loopIterationStart = std::chrono::steady_clock::now();

        // Process events from queue.
        auto eventProcessStart = std::chrono::steady_clock::now();
        eventProcessor.processEventsFromQueue(*this);
        auto eventProcessEnd = std::chrono::steady_clock::now();

        // Tick the simulation if in SimRunning state.
        if (std::holds_alternative<State::SimRunning>(fsmState)) {
            auto& simRunning = std::get<State::SimRunning>(fsmState);

            // Record frame start time for frame limiting.
            auto frameStart = std::chrono::steady_clock::now();

            // Advance simulation.
            simRunning.tick(*this);

            auto frameEnd = std::chrono::steady_clock::now();

            // Log timing breakdown every 1000 frames.
            static int frameCount = 0;
            static double totalEventProcessMs = 0.0;
            static double totalTickMs = 0.0;
            static double totalSleepMs = 0.0;
            static double totalIterationMs = 0.0;

            double eventProcessMs =
                std::chrono::duration<double, std::milli>(eventProcessEnd - eventProcessStart)
                    .count();
            double tickMs =
                std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();

            totalEventProcessMs += eventProcessMs;
            totalTickMs += tickMs;

            // Apply frame rate limiting if configured.
            double sleepMs = 0.0;
            if (simRunning.frameLimit > 0) {
                auto elapsedMs =
                    std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart)
                        .count();

                int remainingMs = simRunning.frameLimit - static_cast<int>(elapsedMs);
                if (remainingMs > 0) {
                    auto sleepStart = std::chrono::steady_clock::now();
                    std::this_thread::sleep_for(std::chrono::milliseconds(remainingMs));
                    auto sleepEnd = std::chrono::steady_clock::now();
                    sleepMs =
                        std::chrono::duration<double, std::milli>(sleepEnd - sleepStart).count();
                    totalSleepMs += sleepMs;
                }
            }

            auto loopIterationEnd = std::chrono::steady_clock::now();
            double iterationMs =
                std::chrono::duration<double, std::milli>(loopIterationEnd - loopIterationStart)
                    .count();
            totalIterationMs += iterationMs;

            frameCount++;
            if (frameCount % 500 == 0) {
                spdlog::info("Main loop timing (avg over {} frames):", frameCount);
                spdlog::info("  Event processing: {:.2f}ms", totalEventProcessMs / frameCount);
                spdlog::info("  Simulation tick: {:.2f}ms", totalTickMs / frameCount);
                spdlog::info("  Sleep: {:.2f}ms", totalSleepMs / frameCount);
                spdlog::info("  Total iteration: {:.2f}ms", totalIterationMs / frameCount);
                spdlog::info(
                    "  Unaccounted: {:.2f}ms",
                    (totalIterationMs - totalEventProcessMs - totalTickMs - totalSleepMs)
                        / frameCount);
            }

            // If frameLimit == 0, no sleep (run as fast as possible).
        }
        else {
            // Small sleep when not running to prevent busy waiting.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
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
                        if constexpr (requires {
                                          evt.sendResponse(std::declval<typename std::decay_t<
                                                               decltype(evt)>::Response>());
                                      }) {
                            auto errorMsg = std::string("Command not supported in state: ")
                                + State::getCurrentStateName(fsmState);
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
