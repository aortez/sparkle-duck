#pragma once

#include "uism/Event.h"
#include "uism/states/State.h"
#include "../core/StateMachineBase.h"
#include "../core/StateMachineInterface.h"
#include "EventProcessor.h"
#include <memory>
#include <string>

struct lv_disp_t;

namespace DirtSim {
namespace Ui {

class StateMachine : public StateMachineBase, public StateMachineInterface<Event> {
public:
    explicit StateMachine(lv_disp_t* display);
    ~StateMachine();

    void mainLoopRun();
    void queueEvent(const Event& event) override;
    void handleEvent(const Event& event);

    std::string getCurrentStateName() const override;
    void processEvents() override;

    lv_disp_t* display = nullptr;
    EventProcessor eventProcessor;

    // TODO: Add WebSocket client (to connect to DSSM server).
    // TODO: Add WebSocket server (to accept remote commands).

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
