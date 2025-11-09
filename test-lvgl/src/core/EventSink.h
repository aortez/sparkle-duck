#pragma once

namespace DirtSim {

/**
 * @brief Generic interface for sending events to a state machine.
 *
 * This interface allows components to send events without depending on
 * the full StateMachine implementation. Enables dependency inversion.
 *
 * @tparam EventType The event variant type for this sink.
 */
template <typename EventType>
class EventSink {
public:
    virtual ~EventSink() = default;

    /**
     * @brief Queue an event for processing.
     * @param event Event to queue.
     */
    virtual void queueEvent(const EventType& event) = 0;
};

} // namespace DirtSim
