#pragma once

#include "Event.h"
#include <type_traits>

/**
 * @brief Trait system to classify events as immediate or queued.
 *
 * This allows compile-time routing of events to the appropriate
 * processing path based on their type.
 */

/**
 * @brief Base template - events are queued by default.
 */
template <typename T>
struct IsImmediateEvent : std::false_type {};

// =================================================================
// IMMEDIATE EVENT SPECIALIZATIONS
// =================================================================

/**
 * @brief GetFPSCommand is processed immediately for low latency.
 */
template <>
struct IsImmediateEvent<GetFPSCommand> : std::true_type {};

/**
 * @brief GetSimStatsCommand is processed immediately for UI updates.
 */
template <>
struct IsImmediateEvent<GetSimStatsCommand> : std::true_type {};

/**
 * @brief PrintAsciiDiagramCommand is processed immediately for UI response.
 */
template <>
struct IsImmediateEvent<PrintAsciiDiagramCommand> : std::true_type {};

/**
 * @brief ToggleForceCommand is processed immediately for UI updates.
 */
template <>
struct IsImmediateEvent<ToggleForceCommand> : std::true_type {};

/**
 * @brief ToggleCohesionCommand is processed immediately for UI updates.
 */
template <>
struct IsImmediateEvent<ToggleCohesionCommand> : std::true_type {};

/**
 * @brief ToggleCohesionForceCommand is processed immediately for UI updates.
 */
template <>
struct IsImmediateEvent<ToggleCohesionForceCommand> : std::true_type {};

/**
 * @brief ToggleAdhesionCommand is processed immediately for UI updates.
 */
template <>
struct IsImmediateEvent<ToggleAdhesionCommand> : std::true_type {};

/**
 * @brief ToggleTimeHistoryCommand is processed immediately for UI updates.
 */
template <>
struct IsImmediateEvent<ToggleTimeHistoryCommand> : std::true_type {};

// NOTE: PauseCommand and ResumeCommand are NOT immediate events
// They need to go through the state machine to trigger state transitions
// /**
//  * @brief PauseCommand is processed immediately for responsive UI.
//  */
// template<>
// struct IsImmediateEvent<PauseCommand> : std::true_type {};
//
// /**
//  * @brief ResumeCommand is processed immediately for responsive UI.
//  */
// template<>
// struct IsImmediateEvent<ResumeCommand> : std::true_type {};

// =================================================================
// HELPER TEMPLATES
// =================================================================

/**
 * @brief Convenience variable template.
 */
template <typename T>
inline constexpr bool is_immediate_event_v = IsImmediateEvent<T>::value;

/**
 * @brief Check if an event variant contains an immediate event.
 */
inline bool isImmediateEvent(const Event& event)
{
    return std::visit(
        [](auto&& e) {
            using T = std::decay_t<decltype(e)>;
            return is_immediate_event_v<T>;
        },
        event);
}