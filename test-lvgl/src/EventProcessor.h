#ifndef EVENTPROCESSOR_H
#define EVENTPROCESSOR_H

#include "Event.h"
#include "SynchronizedQueue.h"
#include <memory>
#include <chrono>

// Forward declarations
namespace DirtSim {
    class DirtSimStateMachine;
}

/**
 * EventProcessor handles processing of queued events from the simulation thread.
 * This class manages the event queue and dispatches events to the state machine.
 */
class EventProcessor {
private:
    SynchronizedQueue<Event> eventQueue;
    
public:
    EventProcessor() = default;
    
    /**
     * Add an event to the queue for processing on the simulation thread.
     * Thread-safe - can be called from any thread.
     */
    void queueEvent(const Event& event);
    
    /**
     * Process all pending events from the queue.
     * Should only be called from the simulation thread.
     * @param dsm The state machine to dispatch events to
     */
    void processEventsFromQueue(DirtSim::DirtSimStateMachine& dsm);
    
    /**
     * Process a single event by dispatching it to the current state.
     * @param event The event to process
     * @param dsm The state machine to dispatch to
     */
    void processEvent(const Event& event, DirtSim::DirtSimStateMachine& dsm);
    
    /**
     * Check if there are events waiting to be processed.
     * Thread-safe.
     */
    bool hasEvents() const { return !eventQueue.empty(); }
    
    /**
     * Get the current queue size.
     * Thread-safe.
     */
    size_t queueSize() const { return eventQueue.size(); }
    
    /**
     * Clear all pending events from the queue.
     * Thread-safe.
     */
    void clearQueue() { eventQueue.clear(); }
    
    /**
     * Get reference to the event queue.
     * Needed for EventRouter initialization.
     */
    SynchronizedQueue<Event>& getEventQueue() { return eventQueue; }
};

#endif // EVENTPROCESSOR_H