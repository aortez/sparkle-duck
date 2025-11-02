#pragma once

#include "events/Event.h"
#include "../core/EventProcessor.h"
#include "../core/StateMachineBase.h"
#include <memory>
#include <string>
#include <variant>

struct lv_disp_t;

namespace DirtSim {
namespace Ui {

namespace State {
struct Startup;
struct MainMenu;
struct SimRunning;
struct Paused;
struct Config;
struct Shutdown;

using Any = std::variant<
    Startup,
    MainMenu,
    SimRunning,
    Paused,
    Config,
    Shutdown
>;

std::string getCurrentStateName(const Any& state);
} // namespace State

class StateMachine : public StateMachineBase {
public:
    explicit StateMachine(lv_disp_t* display);
    ~StateMachine();

    void mainLoopRun();
    void queueEvent(const Event& event);
    void handleEvent(const Event& event);

    std::string getCurrentStateName() const override;
    void processEvents() override;

    lv_disp_t* display = nullptr;
    EventProcessor<Event, StateMachine> eventProcessor;

private:
    State::Any currentState{ State::Startup{} };

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
