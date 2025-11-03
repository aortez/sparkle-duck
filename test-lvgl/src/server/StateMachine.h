#pragma once

#include "Event.h"
#include "EventProcessor.h"
#include "../core/StateMachineBase.h"
#include "../core/StateMachineInterface.h"
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
    void queueEvent(const Event& event);


    /**
     * @brief Handle an event by dispatching to current state.
     * Called by EventProcessor.
     */
    void handleEvent(const Event& event);

    std::string getCurrentStateName() const override { return State::getCurrentStateName(fsmState); }
    void processEvents();

    EventProcessor eventProcessor;

    uint32_t defaultWidth = 28;
    uint32_t defaultHeight = 28;

    // WebSocket server (public so states can access for broadcasting).
    class WebSocketServer* wsServer_ = nullptr;

    /**
     * @brief Get WebSocket server for broadcasting frame notifications.
     * @return Pointer to WebSocket server.
     */
    class WebSocketServer* getWebSocketServer() { return wsServer_; }

    /**
     * @brief Set WebSocket server (called from main).
     * @param server Pointer to WebSocket server.
     */
    void setWebSocketServer(class WebSocketServer* server) { wsServer_ = server; }

private:
    State::Any fsmState{ State::Startup{} };

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