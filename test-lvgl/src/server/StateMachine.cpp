#include "StateMachine.h"
#include "../core/World.h"
#include "../core/WorldEventGenerator.h"
#include "scenarios/Scenario.h"
#include "scenarios/ScenarioRegistry.h"
#include <cassert>
#include <chrono>
#include <spdlog/spdlog.h>
#include <thread>

namespace DirtSim {
namespace Server {

StateMachine::StateMachine() : eventProcessor(), sharedState()
{
    spdlog::info("Server::StateMachine initialized in headless mode in state: {}", getCurrentStateName());

    // Create World directly.
    world = std::make_unique<World>(defaultWidth, defaultHeight);
    if (!world) {
        throw std::runtime_error("Failed to create world");
    }
    spdlog::info("Server::StateMachine: Created {}x{} World", defaultWidth, defaultHeight);

    // Apply default Sandbox scenario if available.
    auto& registry = ScenarioRegistry::getInstance();
    auto* sandboxScenario = registry.getScenario("sandbox");

    if (sandboxScenario) {
        spdlog::info("Applying default Sandbox scenario");
        auto setup = sandboxScenario->createWorldEventGenerator();
        world->setWorldEventGenerator(std::move(setup));
    }
    else {
        spdlog::warn("Sandbox scenario not found - using default world setup");
    }

    // Initialize world.
    world->setup();

    // Set world in SharedSimState for immediate event handlers.
    sharedState.setCurrentWorld(world.get());
    spdlog::info("Server::StateMachine: World registered in SharedSimState");
}

StateMachine::~StateMachine()
{
    spdlog::info("Server::StateMachine shutting down from state: {}", getCurrentStateName());
}

bool StateMachine::resizeWorldIfNeeded(uint32_t requiredWidth, uint32_t requiredHeight)
{
    // If no specific dimensions required, restore default dimensions.
    if (requiredWidth == 0 || requiredHeight == 0) {
        requiredWidth = defaultWidth;
        requiredHeight = defaultHeight;
    }

    // Check if current dimensions match.
    uint32_t currentWidth = world ? world->getWidth() : 0;
    uint32_t currentHeight = world ? world->getHeight() : 0;

    if (currentWidth == requiredWidth && currentHeight == requiredHeight) {
        return false;
    }

    spdlog::info(
        "Resizing world from {}x{} to {}x{} for scenario",
        currentWidth,
        currentHeight,
        requiredWidth,
        requiredHeight);

    // Create new world with new dimensions.
    world = std::make_unique<World>(requiredWidth, requiredHeight);
    if (!world) {
        spdlog::error("Failed to create resized world");
        return false;
    }

    spdlog::info("World resized successfully to {}x{}", requiredWidth, requiredHeight);
    return true;
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

        // Small sleep to prevent busy waiting.
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS.
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

void StateMachine::processImmediateEvent(const Event& event, SharedSimState& shared)
{
    // Immediate events are processed directly without state dispatch.
    std::visit(
        [this, &shared](const auto& e) {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, GetFPSCommand>) {
                // Already handled by EventRouter.
            }
            else if constexpr (std::is_same_v<T, GetSimStatsCommand>) {
                // Already handled by EventRouter.
            }
            else if constexpr (std::is_same_v<T, PauseCommand>) {
                // Already handled by EventRouter.
            }
            else if constexpr (std::is_same_v<T, ResumeCommand>) {
                // Already handled by EventRouter.
            }
        },
        event);
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
                    }
                    else {
                        spdlog::warn(
                            "Server::StateMachine: State {} does not handle event {}",
                            State::getCurrentStateName(fsmState),
                            getEventName(Event{ evt }));
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

    // Push UI update on state transitions (always enabled for thread safety).
    UiUpdateEvent update = buildUIUpdate();
    sharedState.pushUIUpdate(std::move(update));
}

// Global event handlers.

State::Any StateMachine::onEvent(const QuitApplicationCommand& /*cmd.*/)
{
    spdlog::info("Global handler: QuitApplicationCommand received");
    sharedState.setShouldExit(true);
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

UiUpdateEvent StateMachine::buildUIUpdate()
{
    assert(world && "World must exist when building UI update");

    UiUpdateEvent update;

    // Sequence tracking.
    update.sequenceNum = sharedState.getNextUpdateSequence();

    // Copy complete world state.
    update.world = *dynamic_cast<World*>(world.get());

    // Simulation metadata.
    update.fps = static_cast<uint32_t>(sharedState.getCurrentFPS());
    update.stepCount = sharedState.getCurrentStep();

    // UI-only state.
    update.isPaused = sharedState.getIsPaused();

    // Timestamp.
    update.timestamp = std::chrono::steady_clock::now();

    return update;
}

} // namespace Server
} // namespace DirtSim
