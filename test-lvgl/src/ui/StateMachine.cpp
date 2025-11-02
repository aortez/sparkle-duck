#include "StateMachine.h"
#include "states/State.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

StateMachine::StateMachine(lv_disp_t* disp)
    : display(disp)
{
    spdlog::info("Ui::StateMachine: Created");
}

StateMachine::~StateMachine()
{
    spdlog::info("Ui::StateMachine: Destroyed");
}

void StateMachine::mainLoopRun()
{
    spdlog::info("Ui::StateMachine: Starting main loop");
    while (!shouldExit()) {
        processEvents();
        // TODO: LVGL event processing integration.
    }
    spdlog::info("Ui::StateMachine: Main loop exited");
}

void StateMachine::queueEvent(const Event& event)
{
    spdlog::debug("Ui::StateMachine: Event queued: {}", getEventName(event));
    eventProcessor.enqueueEvent(event);
}

void StateMachine::handleEvent(const Event& event)
{
    spdlog::trace("Ui::StateMachine: Handling event: {}", getEventName(event));

    std::visit([this](auto&& evt) {
        using EventType = std::decay_t<decltype(evt)>;

        std::visit([this, &evt](auto&& state) -> void {
            using StateType = std::decay_t<decltype(state)>;

            if constexpr (requires { state.onEvent(evt, *this); }) {
                auto newState = state.onEvent(evt, *this);
                if (!std::holds_alternative<StateType>(newState)) {
                    transitionTo(std::move(newState));
                }
            } else {
                spdlog::trace("Ui::StateMachine: State {} does not handle event {}",
                    State::getCurrentStateName(currentState), getEventName(Event{evt}));
            }
        }, currentState);
    }, event);
}

void StateMachine::processEvents()
{
    eventProcessor.processEventsFromQueue(*this);
}

std::string StateMachine::getCurrentStateName() const
{
    return State::getCurrentStateName(currentState);
}

void StateMachine::transitionTo(State::Any newState)
{
    std::string oldStateName = State::getCurrentStateName(currentState);

    std::visit([this](auto&& state) { callOnExit(state); }, currentState);

    currentState = std::move(newState);

    std::string newStateName = State::getCurrentStateName(currentState);
    spdlog::info("Ui::StateMachine: {} -> {}", oldStateName, newStateName);

    std::visit([this](auto&& state) { callOnEnter(state); }, currentState);
}

} // namespace Ui
} // namespace DirtSim
