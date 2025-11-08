#pragma once

#include "Event.h"

namespace DirtSim {
namespace Ui {

/**
 * @brief Interface for sending UI events.
 *
 * This interface allows components like ControlPanel to send events
 * without depending on the full StateMachine implementation.
 */
class EventSink {
public:
    virtual ~EventSink() = default;

    /**
     * @brief Queue an event for processing.
     * @param event Event to queue.
     */
    virtual void queueEvent(const Event& event) = 0;
};

} // namespace Ui
} // namespace DirtSim
