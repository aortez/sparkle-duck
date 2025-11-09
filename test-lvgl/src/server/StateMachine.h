#pragma once

#include "Event.h"
#include "EventProcessor.h"
#include "scenarios/ScenarioRegistry.h"
#include "core/StateMachineBase.h"
#include "core/StateMachineInterface.h"
#include "core/Timers.h"
#include "states/State.h"
#include <functional>
#include <memory>
#include <mutex>

namespace DirtSim {

struct WorldData;  // Forward declaration.

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

    // Cached WorldData for fast state_get responses (shared between physics and WebSocket threads).
    std::shared_ptr<WorldData> cachedWorldData_;
    mutable std::mutex cachedWorldDataMutex_;

    /**
     * @brief Update cached WorldData (called by SimRunning after physics step).
     * @param data New WorldData to cache.
     */
    void updateCachedWorldData(const WorldData& data);

    /**
     * @brief Get cached WorldData (thread-safe, called by state_get handler).
     * @return Shared pointer to cached WorldData (may be null if no data yet).
     */
    std::shared_ptr<WorldData> getCachedWorldData() const;

    /**
     * @brief Get scenario registry for accessing scenarios.
     * @return Reference to scenario registry.
     */
    ScenarioRegistry& getScenarioRegistry() { return scenarioRegistry_; }

    /**
     * @brief Get performance timers for instrumentation.
     * @return Reference to timers.
     */
    Timers& getTimers() { return timers_; }

private:
    ScenarioRegistry scenarioRegistry_;  // Owned scenario registry.
    Timers timers_;  // Performance instrumentation timers.
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
};

} // namespace Server
} // namespace DirtSim
