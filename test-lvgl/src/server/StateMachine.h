#pragma once

#include "Event.h"
#include "EventProcessor.h"
#include "../core/SharedSimState.h"
#include "../core/StateMachineBase.h"
#include "../core/StateMachineInterface.h"
#include "../core/WorldInterface.h"
#include "states/State.h"
#include <functional>
#include <memory>

namespace DirtSim {
namespace Server {

class StateMachine : public StateMachineBase, public StateMachineInterface<Event> {
public:
    StateMachine();
    ~StateMachine();

    void mainLoopRun();
    void queueEvent(const Event& event) override;

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

    std::string getCurrentStateName() const override { return State::getCurrentStateName(fsmState); }
    void processEvents() override;

    SharedSimState& getSharedState() { return sharedState; }
    bool resizeWorldIfNeeded(uint32_t requiredWidth, uint32_t requiredHeight);
    UiUpdateEvent buildUIUpdate();

    std::unique_ptr<WorldInterface> world;
    EventProcessor eventProcessor;

    uint32_t defaultWidth = 28;
    uint32_t defaultHeight = 28;

private:
    State::Any fsmState{ State::Startup{} };
    SharedSimState sharedState;

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

    friend class EventDispatcher;
};

} // namespace Server
} // namespace DirtSim