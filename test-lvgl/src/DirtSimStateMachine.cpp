#include "DirtSimStateMachine.h"
#include "EventDispatcher.h"
#include "SimulationManager.h"
#include "WorldFactory.h"
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
    // Initialize UIManager if display is available.
    if (display) {
        uiManager = std::make_unique<UIManager>(display);
        spdlog::info(
            "DirtSimStateMachine initialized with UI support in state: {}", getCurrentStateName());
    }
    else {
        spdlog::info(
            "DirtSimStateMachine initialized in headless mode in state: {}", getCurrentStateName());
    }

    // Create SimulationManager upfront with default settings.
    lv_obj_t* screen = nullptr;
    if (lv_is_initialized() && display) {
        screen = lv_scr_act();
    }

    // Default grid size (matches main.cpp calculation).
    const int grid_width = 7;                // (850 / 100) - 1, where 100 is Cell::WIDTH.
    const int grid_height = 7;               // (850 / 100) - 1, where 100 is Cell::HEIGHT.
    WorldType worldType = WorldType::RulesB; // Default.

    simulationManager = std::make_unique<SimulationManager>(
        worldType, grid_width, grid_height, screen, eventRouter.get());

    simulationManager->initialize();

    // Set world in SharedSimState for immediate event handlers.
    if (simulationManager->getWorld()) {
        sharedState.setCurrentWorld(simulationManager->getWorld());
        spdlog::info("DirtSimStateMachine: World registered in SharedSimState");
    }

    spdlog::info("DirtSimStateMachine: SimulationManager created and initialized");
}

DirtSimStateMachine::~DirtSimStateMachine()
{
    spdlog::info("DirtSimStateMachine shutting down from state: {}", getCurrentStateName());
}

void DirtSimStateMachine::mainLoopRun()
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

void DirtSimStateMachine::queueEvent(const Event& event)
{
    eventProcessor.queueEvent(event);
}

void DirtSimStateMachine::processImmediateEvent(const Event& event, SharedSimState& shared)
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

void DirtSimStateMachine::handleEvent(const Event& event)
{
    // Save the current state index before moving.
    std::size_t oldStateIndex = fsmState.index();

    // Use EventDispatcher to route to current state.
    // Move the current state to dispatch.
    State::Any newState = EventDispatcher::dispatch(std::move(fsmState), event, *this);

    // Check if the state type changed.
    if (newState.index() != oldStateIndex) {
        // State type changed - use transitionTo to handle lifecycle.
        transitionTo(std::move(newState));
    }
    else {
        // Same state type - just update without lifecycle calls.
        fsmState = std::move(newState);
    }
}

void DirtSimStateMachine::transitionTo(State::Any newState)
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

    // Push UI update on state transitions if push-based system is enabled.
    if (sharedState.isPushUpdatesEnabled()) {
        // Build update with uiState dirty flag forced on for state changes.
        UIUpdateEvent update = buildUIUpdate();
        update.dirty.uiState = true; // Always mark UI state dirty on transitions.
        sharedState.pushUIUpdate(std::move(update));
    }
}

// Global event handlers.

State::Any DirtSimStateMachine::onEvent(const QuitApplicationCommand& /*cmd.*/)
{
    spdlog::info("Global handler: QuitApplicationCommand received");
    sharedState.setShouldExit(true);
    return State::Shutdown{};
}

State::Any DirtSimStateMachine::onEvent(const GetFPSCommand& /*cmd.*/)
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

State::Any DirtSimStateMachine::onEvent(const GetSimStatsCommand& /*cmd.*/)
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

UIUpdateEvent DirtSimStateMachine::buildUIUpdate()
{
    UIUpdateEvent update;

    // Sequence tracking.
    update.sequenceNum = sharedState.getNextUpdateSequence();

    // Core simulation data.
    update.fps = static_cast<uint32_t>(sharedState.getCurrentFPS());
    update.stepCount = sharedState.getCurrentStep();
    update.stats = sharedState.getStats();

    // Physics parameters - read from world (source of truth).
    if (simulationManager && simulationManager->getWorld()) {
        auto* world = simulationManager->getWorld();
        update.physicsParams.gravity = world->getGravity();
        update.physicsParams.elasticity = world->getElasticityFactor();
        update.physicsParams.timescale = world->getTimescale();
        update.debugEnabled = world->isDebugDrawEnabled();
        update.forceEnabled = world->isCursorForceEnabled();
        update.cohesionEnabled = world->isCohesionComForceEnabled();
        update.adhesionEnabled = world->isAdhesionEnabled();
        update.timeHistoryEnabled = world->isTimeReversalEnabled();
    }

    // UI state.
    update.isPaused = sharedState.getIsPaused();

    // World state.
    update.selectedMaterial = sharedState.getSelectedMaterial();

    // Get world type string.
    if (simulationManager) {
        WorldType currentType = simulationManager->getCurrentWorldType();
        update.worldType = getWorldTypeName(currentType);
    }
    else {
        update.worldType = "None";
    }

    // Timestamp.
    update.timestamp = std::chrono::steady_clock::now();

    // TODO: Set dirty flags based on tracking previous state.
    // For now, mark everything as dirty.
    update.dirty.fps = true;
    update.dirty.stats = true;
    update.dirty.physicsParams = true;
    update.dirty.uiState = true;
    update.dirty.worldState = true;

    return update;
}

} // namespace DirtSim.
