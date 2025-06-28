#pragma once

#include "Event.h"
#include "EventTraits.h"
#include "SharedSimState.h"
#include "SynchronizedQueue.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <thread>

// Forward declaration.
namespace DirtSim {
class DirtSimStateMachine;
}

/**
 * @brief Routes events to immediate or queued processing based on type.
 *
 * This class implements the dual-path event system, routing events
 * to either immediate processing (UI thread) or queued processing
 * (simulation thread) based on compile-time type information.
 */
class EventRouter {
public:
    /**
     * @brief Construct event router.
     * @param stateMachine Reference to the state machine.
     * @param sharedState Reference to shared simulation state.
     * @param eventQueue Reference to the event queue.
     */
    EventRouter(
        DirtSim::DirtSimStateMachine& stateMachine,
        SharedSimState& sharedState,
        SynchronizedQueue<Event>& eventQueue)
        : stateMachine_(stateMachine), sharedState_(sharedState), eventQueue_(eventQueue)
    {}

    /**
     * @brief Route an event to appropriate processing path.
     * @param event The event to route.
     */
    void routeEvent(const Event& event)
    {
        std::visit(
            [this](auto&& e) {
                using T = std::decay_t<decltype(e)>;

                if constexpr (is_immediate_event_v<T>) {
                    // Check if push updates are enabled and event is compatible
                    if (sharedState_.isPushUpdatesEnabled() && isPushCompatible(e)) {
                        // Route through state machine for push-based update.
                        queueEvent(e);
                        spdlog::debug(
                            "Routing {} through push system instead of immediate", e.name());
                    }
                    else {
                        // Process immediately on current thread (legacy behavior).
                        processImmediate(e);
                    }
                }
                else {
                    // Queue for simulation thread.
                    queueEvent(e);
                }
            },
            event);
    }

    /**
     * @brief Get reference to SharedSimState.
     * @return Reference to shared simulation state.
     */
    SharedSimState& getSharedSimState() { return sharedState_; }

    /**
     * @brief Get pointer to SharedSimState for UIUpdateConsumer.
     * @return Pointer to shared simulation state.
     */
    SharedSimState* getSharedSimStatePtr() { return &sharedState_; }

private:
    /**
     * @brief Check if an immediate event is compatible with push-based updates.
     * @param event The event to check.
     * @return true if the event can be routed through the push system.
     */
    template <typename T>
    bool isPushCompatible(const T& /*event*/) const
    {
        // All current immediate events are push-compatible.
        // They update UI state that can be delivered via push updates.
        return std::is_same_v<T, GetFPSCommand> || std::is_same_v<T, GetSimStatsCommand>
            || std::is_same_v<T, ToggleDebugCommand> || std::is_same_v<T, ToggleForceCommand>
            || std::is_same_v<T, ToggleCohesionCommand> || std::is_same_v<T, ToggleAdhesionCommand>
            || std::is_same_v<T, ToggleTimeHistoryCommand>
            || std::is_same_v<T, PrintAsciiDiagramCommand>;
    }

    /**
     * @brief Process an immediate event on the current thread.
     */
    template <typename T>
    void processImmediate(const T& event)
    {
        auto threadId = std::this_thread::get_id();
        spdlog::info(
            "EVENT_IMMEDIATE: {} [thread: {}]",
            event.name(),
            std::hash<std::thread::id>{}(threadId));

        auto start = std::chrono::steady_clock::now();

        // Process the event immediately.
        processImmediateEvent(event);

        auto duration = std::chrono::steady_clock::now() - start;
        auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();

        spdlog::info("EVENT_IMMEDIATE: {} processed in {} us", event.name(), durationUs);
    }

    /**
     * @brief Queue an event for simulation thread processing.
     */
    template <typename T>
    void queueEvent(const T& event)
    {
        eventQueue_.push(Event(event));
        auto queueDepth = eventQueue_.size();

        auto threadId = std::this_thread::get_id();
        spdlog::info(
            "EVENT_QUEUED: {} [queue_depth: {}, thread: {}]",
            event.name(),
            queueDepth,
            std::hash<std::thread::id>{}(threadId));
    }

    /**
     * @brief Process immediate events based on type.
     */
    void processImmediateEvent(const GetFPSCommand& cmd);
    void processImmediateEvent(const GetSimStatsCommand& cmd);
    void processImmediateEvent(const PauseCommand& cmd);
    void processImmediateEvent(const ResumeCommand& cmd);
    void processImmediateEvent(const ToggleDebugCommand& cmd);
    void processImmediateEvent(const PrintAsciiDiagramCommand& cmd);
    void processImmediateEvent(const ToggleForceCommand& cmd);
    void processImmediateEvent(const ToggleCohesionCommand& cmd);
    void processImmediateEvent(const ToggleAdhesionCommand& cmd);
    void processImmediateEvent(const ToggleTimeHistoryCommand& cmd);

    DirtSim::DirtSimStateMachine& stateMachine_;
    SharedSimState& sharedState_;
    SynchronizedQueue<Event>& eventQueue_;
};