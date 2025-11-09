#pragma once

#include "Event.h"
#include "EventSink.h"
#include "states/State.h"
#include "core/StateMachineBase.h"
#include "core/StateMachineInterface.h"
#include "core/Timers.h"
#include "EventProcessor.h"
#include <memory>
#include <string>

// Forward declaration for LVGL display structure.
struct _lv_display_t;

// Forward declarations for network and UI components.
namespace DirtSim {
class UiComponentManager;
namespace Ui {
class WebSocketServer;
class WebSocketClient;
}
}

namespace DirtSim {
namespace Ui {

class StateMachine : public StateMachineBase, public StateMachineInterface<Event>, public EventSink {
public:
    explicit StateMachine(_lv_display_t* display, uint16_t wsPort = 7070);
    ~StateMachine();

    void mainLoopRun();
    void queueEvent(const Event& event) override;
    void handleEvent(const Event& event);

    std::string getCurrentStateName() const override;
    void processEvents() override;

    _lv_display_t* display = nullptr;
    EventProcessor eventProcessor;

    // WebSocket connections.
    std::unique_ptr<WebSocketServer> wsServer_;  // Server for accepting remote commands.
    std::unique_ptr<WebSocketClient> wsClient_;  // Client for connecting to DSSM server.

    // UI management.
    std::unique_ptr<UiComponentManager> uiManager_;  // LVGL screen and container management.

    /**
     * @brief Get WebSocket client for DSSM connection.
     * @return Pointer to WebSocket client (non-owning).
     */
    WebSocketClient* getWebSocketClient() { return wsClient_.get(); }

    /**
     * @brief Get UI manager for LVGL screen/container access.
     * @return Pointer to UI manager (non-owning).
     */
    UiComponentManager* getUiComponentManager() { return uiManager_.get(); }

    /**
     * @brief Get performance timers for instrumentation.
     * @return Reference to timers.
     */
    Timers& getTimers() { return timers_; }

private:
    Timers timers_;  // Performance instrumentation timers.
    State::Any fsmState{ State::Startup{} };

    void transitionTo(State::Any newState);

    template <typename StateType>
    void callOnEnter(StateType& state)
    {
        if constexpr (requires { state.onEnter(*this); }) {
            state.onEnter(*this);
        }
    }

    template <typename StateType>
    void callOnExit(StateType& state)
    {
        if constexpr (requires { state.onExit(*this); }) {
            state.onExit(*this);
        }
    }
};

} // namespace Ui
} // namespace DirtSim
