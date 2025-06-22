#pragma once

#include "Event.h"
#include "EventProcessor.h"
#include "EventRouter.h"
#include "SharedSimState.h"
#include "SimulationManager.h"
#include "UIManager.h"
#include "WorldInterface.h"
#include "states/State.h"
#include <functional>
#include <memory>

namespace DirtSim {

/**
 * @brief Main state machine for the Dirt Sim application.
 *
 * Manages application states, event processing, and coordination between
 * UI and physics simulation.
 */
class DirtSimStateMachine {
public:
    /**
     * @brief Construct with optional display for UI.
     * @param display LVGL display for UI (can be nullptr for headless)
     */
    explicit DirtSimStateMachine(lv_disp_t* display = nullptr);
    ~DirtSimStateMachine();

    /**
     * @brief Initialize and run the main event loop.
     */
    void mainLoopRun();

    /**
     * @brief Queue an event for processing on the simulation thread.
     */
    void queueEvent(const Event& event);

    /**
     * @brief Process an event immediately (for immediate events).
     * Should only be called from EventRouter.
     */
    void processImmediateEvent(const Event& event, SharedSimState& shared);

    /**
     * @brief Handle an event by dispatching to current state.
     * Called by EventProcessor.
     */
    void handleEvent(const Event& event);

    /**
     * @brief Get the current state name for logging.
     */
    std::string getCurrentStateName() const { return State::getCurrentStateName(fsmState); }

    /**
     * @brief Check if we should exit.
     */
    bool shouldExit() const { return sharedState.getShouldExit(); }

    /**
     * @brief Get the event router.
     */
    EventRouter& getEventRouter() { return *eventRouter; }

    /**
     * @brief Get the shared state.
     */
    SharedSimState& getSharedState() { return sharedState; }

    /**
     * @brief Get the current SimulationManager for backend loop integration.
     * @return Pointer to SimulationManager or nullptr if not in simulation state
     */
    SimulationManager* getSimulationManager() { return simulationManager.get(); }

    // Public members for state access
    std::unique_ptr<WorldInterface> world;
    lv_disp_t* display = nullptr;
    std::unique_ptr<UIManager> uiManager;
    std::unique_ptr<SimulationManager> simulationManager;
    EventProcessor eventProcessor;

private:
    State::Any fsmState{ State::Startup{} };
    SharedSimState sharedState;
    std::unique_ptr<EventRouter> eventRouter;

    /**
     * @brief Transition to a new state.
     * Handles onExit and onEnter lifecycle calls.
     */
    void transitionTo(State::Any newState);

    /**
     * @brief Call onEnter if the state has it.
     */
    template <typename State>
    void callOnEnter(State& state)
    {
        if constexpr (requires { state.onEnter(*this); }) {
            state.onEnter(*this);
        }
    }

    /**
     * @brief Call onExit if the state has it.
     */
    template <typename State>
    void callOnExit(State& state)
    {
        if constexpr (requires { state.onExit(*this); }) {
            state.onExit(*this);
        }
    }

    // Global event handlers (available in all states)
    State::Any onEvent(const QuitApplicationCommand& cmd);
    State::Any onEvent(const GetFPSCommand& cmd);
    State::Any onEvent(const GetSimStatsCommand& cmd);

    // Allow EventDispatcher to access private event handlers
    friend class EventDispatcher;
};

} // namespace DirtSim