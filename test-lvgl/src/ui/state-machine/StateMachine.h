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

    // TODO: Add WebSocket client (to connect to DSSM server).

private:
    class WebSocketServer* wsServer_ = nullptr; // Owned WebSocket server for remote commands.
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
