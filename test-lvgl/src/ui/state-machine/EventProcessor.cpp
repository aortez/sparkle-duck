#include "EventProcessor.h"
#include "StateMachine.h"
#include "core/SynchronizedQueue.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

struct EventQueue {
    SynchronizedQueue<Event> queue;
};

EventProcessor::EventProcessor() : eventQueue(std::make_shared<EventQueue>())
{}

void EventProcessor::processEvent(StateMachine& sm, const Event& eventVariant)
{
    sm.handleEvent(eventVariant);
}

void EventProcessor::processEventsFromQueue(StateMachine& sm)
{
    while (!eventQueue->queue.empty()) {
        auto event = eventQueue->queue.tryPop();
        if (event.has_value()) {
            spdlog::trace("Ui::EventProcessor: Processing event: {}", getEventName(event.value()));
            processEvent(sm, event.value());
        }
    }
}

void EventProcessor::enqueueEvent(const Event& event)
{
    spdlog::debug("Ui::EventProcessor: Enqueuing event: {}", getEventName(event));
    eventQueue->queue.push(event);
}

bool EventProcessor::hasEvents() const
{
    return !eventQueue->queue.empty();
}

size_t EventProcessor::queueSize() const
{
    return eventQueue->queue.size();
}

void EventProcessor::clearQueue()
{
    eventQueue->queue.clear();
}

} // namespace Ui
} // namespace DirtSim
