#pragma once

#include "Event.h"
#include <memory>

namespace DirtSim {
namespace Ui {

class StateMachine;
struct EventQueue;

class EventProcessor {
public:
    EventProcessor();

    void processEvent(StateMachine& sm, const Event& event);
    void processEventsFromQueue(StateMachine& sm);
    void enqueueEvent(const Event& event);

    bool hasEvents() const;
    size_t queueSize() const;
    void clearQueue();

    std::shared_ptr<EventQueue> eventQueue;
};

} // namespace Ui
} // namespace DirtSim
