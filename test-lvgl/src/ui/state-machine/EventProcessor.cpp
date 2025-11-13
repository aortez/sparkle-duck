#include "EventProcessor.h"
#include "StateMachine.h"
#include "core/SynchronizedQueue.h"
#include <optional>
#include <spdlog/spdlog.h>
#include <vector>

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
    // Frame dropping: If multiple UiUpdateEvents are queued, only process the latest one.
    // This prevents the UI from falling behind and hanging when rendering is slow.
    std::optional<Event> latestUiUpdate;
    std::vector<Event> otherEvents;
    int droppedFrames = 0;

    // First pass: separate UiUpdateEvents from other events
    while (!eventQueue->queue.empty()) {
        auto event = eventQueue->queue.tryPop();
        if (event.has_value()) {
            // Check if this is a UiUpdateEvent
            if (std::holds_alternative<DirtSim::UiUpdateEvent>(event.value())) {
                if (latestUiUpdate.has_value()) {
                    droppedFrames++;  // Drop the previous frame
                }
                latestUiUpdate = event.value();  // Keep the latest
            } else {
                otherEvents.push_back(event.value());
            }
        }
    }

    if (droppedFrames > 0) {
        spdlog::info("Ui::EventProcessor: Dropped {} old frames to catch up (queue overrun)", droppedFrames);
    }

    // Process other events first (commands, etc.)
    for (const auto& event : otherEvents) {
        spdlog::trace("Ui::EventProcessor: Processing event: {}", getEventName(event));
        processEvent(sm, event);
    }

    // Process the latest UI update (if any)
    if (latestUiUpdate.has_value()) {
        spdlog::trace("Ui::EventProcessor: Processing event: {}", getEventName(latestUiUpdate.value()));
        processEvent(sm, latestUiUpdate.value());
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
