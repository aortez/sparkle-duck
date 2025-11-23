#pragma once

#include "core/Pimpl.h"
#include "core/StateMachineBase.h"
#include "core/StateMachineInterface.h"

#include <functional>
#include <memory>

// Forward declarations (global namespace).
class Timers;
class ScenarioRegistry;

// Forward declarations (DirtSim namespace).
namespace DirtSim {
struct WorldData;

namespace Server {

class Event;
class EventProcessor;
class WebSocketServer;
struct QuitApplicationCommand;
struct GetFPSCommand;
struct GetSimStatsCommand;

namespace State {
class Any;
}

class StateMachine : public StateMachineBase, public StateMachineInterface<Event> {
public:
    StateMachine();
    ~StateMachine();

    void mainLoopRun();
    void queueEvent(const Event& event);

    /**
     * @brief Handle an event by dispatching to current state.
     */
    void handleEvent(const Event& event);

    std::string getCurrentStateName() const override;
    void processEvents();

    // Accessor methods for Pimpl members.
    EventProcessor& getEventProcessor();
    const EventProcessor& getEventProcessor() const;

    class WebSocketServer* getWebSocketServer();
    void setWebSocketServer(class WebSocketServer* server);

    void updateCachedWorldData(const WorldData& data);
    std::shared_ptr<WorldData> getCachedWorldData() const;

    ScenarioRegistry& getScenarioRegistry();
    const ScenarioRegistry& getScenarioRegistry() const;

    Timers& getTimers();
    const Timers& getTimers() const;

    uint32_t defaultWidth = 28;
    uint32_t defaultHeight = 28;

private:
    struct Impl;
    Pimpl<Impl> pImpl;

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
};

} // namespace Server
} // namespace DirtSim
