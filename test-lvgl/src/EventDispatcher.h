#pragma once

#include "Event.h"
#include "states/State.h"
#include <spdlog/spdlog.h>

// Forward declaration.
namespace DirtSim {
class DirtSimStateMachine;
}

namespace DirtSim {

/**
 * @brief Dispatches events to the appropriate handler.
 *
 * Uses compile-time detection to check if a state has a handler for
 * a specific event type. Falls back to global handlers if needed.
 */
class EventDispatcher {
public:
    /**
     * @brief Dispatch an event to the current state.
     * @return The new state (may be the same as current).
     */
    static State::Any dispatch(
        State::Any currentState, const Event& event, DirtSimStateMachine& dsm)
    {
        return std::visit(
            [&event, &dsm](auto&& state) -> State::Any {
                using StateType = std::decay_t<decltype(state)>;

                return std::visit(
                    [&state, &dsm](auto&& evt) -> State::Any {
                        using EventType = std::decay_t<decltype(evt)>;

                        // Special case for QuitApplicationCommand - always handled globally.
                        if constexpr (std::is_same_v<EventType, QuitApplicationCommand>) {
                            spdlog::info("Dispatching QuitApplicationCommand to global handler");
                            return dsm.onEvent(evt);
                        }

                        // Try state-specific handler first.
                        if constexpr (hasEventHandler<StateType, EventType>()) {
                            if constexpr (std::is_same_v<EventType, AdvanceSimulationCommand>) {
                                spdlog::debug(
                                    "Dispatching {} to state handler in {}",
                                    EventType::name(),
                                    StateType::name());
                            }
                            else {
                                spdlog::info(
                                    "Dispatching {} to state handler in {}",
                                    EventType::name(),
                                    StateType::name());
                            }
                            return state.onEvent(evt, dsm);
                        }
                        // Try global handler.
                        else if constexpr (hasGlobalHandler<EventType>()) {
                            spdlog::info(
                                "Dispatching {} to global handler from {}",
                                EventType::name(),
                                StateType::name());
                            return dsm.onEvent(evt);
                        }
                        // No handler found.
                        else {
                            spdlog::info(
                                "No handler for {} in state {}",
                                EventType::name(),
                                StateType::name());
                            return state;
                        }
                    },
                    event);
            },
            currentState);
    }

private:
    /**
     * @brief Check if a state has a handler for an event type.
     */
    template <typename State, typename Event>
    static constexpr bool hasEventHandler()
    {
        return requires(State& s, const Event& e, DirtSimStateMachine& dsm) {
            { s.onEvent(e, dsm) } -> std::same_as<DirtSim::State::Any>;
        };
    }

    /**
     * @brief Check if DirtSimStateMachine has a global handler for an event type.
     */
    template <typename Event>
    static constexpr bool hasGlobalHandler()
    {
        // Check if DirtSimStateMachine has a method onEvent that takes const Event&
        // and returns State::Any.
        if constexpr (requires(DirtSimStateMachine& dsm, const Event& e) {
                          { dsm.onEvent(e) } -> std::same_as<DirtSim::State::Any>;
                      }) {
            return true;
        }
        return false;
    }
};

} // namespace DirtSim