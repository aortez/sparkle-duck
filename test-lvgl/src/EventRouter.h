#pragma once

#include "Event.h"
#include "EventTraits.h"
#include "SharedSimState.h"
#include "SynchronizedQueue.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <thread>

// Forward declaration
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
    EventRouter(DirtSim::DirtSimStateMachine& stateMachine, 
                SharedSimState& sharedState,
                SynchronizedQueue<Event>& eventQueue)
        : stateMachine_(stateMachine)
        , sharedState_(sharedState)
        , eventQueue_(eventQueue) {}
    
    /**
     * @brief Route an event to appropriate processing path.
     * @param event The event to route.
     */
    void routeEvent(const Event& event) {
        std::visit([this](auto&& e) {
            using T = std::decay_t<decltype(e)>;
            
            if constexpr (is_immediate_event_v<T>) {
                // Process immediately on current thread
                processImmediate(e);
            } else {
                // Queue for simulation thread
                queueEvent(e);
            }
        }, event);
    }
    
private:
    /**
     * @brief Process an immediate event on the current thread.
     */
    template<typename T>
    void processImmediate(const T& event) {
        auto threadId = std::this_thread::get_id();
        spdlog::info("EVENT_IMMEDIATE: {} [thread: {}]", event.name(), 
                     std::hash<std::thread::id>{}(threadId));
        
        auto start = std::chrono::steady_clock::now();
        
        // Process the event immediately
        processImmediateEvent(event);
        
        auto duration = std::chrono::steady_clock::now() - start;
        auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        
        spdlog::info("EVENT_IMMEDIATE: {} processed in {} us", event.name(), durationUs);
    }
    
    /**
     * @brief Queue an event for simulation thread processing.
     */
    template<typename T>
    void queueEvent(const T& event) {
        eventQueue_.push(Event(event));
        auto queueDepth = eventQueue_.size();
        
        auto threadId = std::this_thread::get_id();
        spdlog::info("EVENT_QUEUED: {} [queue_depth: {}, thread: {}]", 
                     event.name(), queueDepth, std::hash<std::thread::id>{}(threadId));
    }
    
    /**
     * @brief Process immediate events based on type.
     */
    void processImmediateEvent(const GetFPSCommand& cmd);
    void processImmediateEvent(const GetSimStatsCommand& cmd);
    void processImmediateEvent(const PauseCommand& cmd);
    void processImmediateEvent(const ResumeCommand& cmd);
    
    DirtSim::DirtSimStateMachine& stateMachine_;
    SharedSimState& sharedState_;
    SynchronizedQueue<Event>& eventQueue_;
};