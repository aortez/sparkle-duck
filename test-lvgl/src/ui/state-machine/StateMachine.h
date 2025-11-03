#pragma once

#include "Event.h"
#include "states/State.h"
#include "../../core/StateMachineBase.h"
#include "../../core/StateMachineInterface.h"
#include "EventProcessor.h"
#include <memory>
#include <string>

// Forward declaration for LVGL display structure.
struct _lv_display_t;

// Forward declarations for network components.
namespace DirtSim {
namespace Ui {
class WebSocketServer;
class WebSocketClient;
}
}

namespace DirtSim {
namespace Ui {

class StateMachine : public StateMachineBase, public StateMachineInterface<Event> {
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
    WebSocketServer* wsServer_ = nullptr;  // Server for accepting remote commands.
    WebSocketClient* wsClient_ = nullptr;  // Client for connecting to DSSM server.

    /**
     * @brief Get WebSocket client for DSSM connection.
     * @return Pointer to WebSocket client.
     */
    WebSocketClient* getWebSocketClient() { return wsClient_; }

private:
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
