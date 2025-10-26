#include "EventProcessor.h"
#include "DirtSimStateMachine.h"
#include <chrono>
#include <spdlog/spdlog.h>
#include <sstream>

void EventProcessor::queueEvent(const Event& event)
{
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    spdlog::debug(
        "EVENT_QUEUED: {} [queue_depth: {}, thread: {}]",
        getEventName(event),
        eventQueue.size() + 1,
        oss.str());

    eventQueue.push(event);
}

void EventProcessor::processEventsFromQueue(DirtSim::DirtSimStateMachine& dsm)
{
    while (auto eventOpt = eventQueue.tryPop()) {
        processEvent(*eventOpt, dsm);

        // Check if we should exit after processing this event.
        if (dsm.shouldExit()) {
            spdlog::info("EventProcessor: Exiting due to shouldExit flag.");
            break;
        }
    }
}

void EventProcessor::processEvent(const Event& event, DirtSim::DirtSimStateMachine& dsm)
{
    auto startTime = std::chrono::steady_clock::now();

    std::string eventName = getEventName(event);
    if (eventName == "AdvanceSimulationCommand") {
        spdlog::debug("Processing event: {} in state: {}", eventName, dsm.getCurrentStateName());
    } else {
        spdlog::info("Processing event: {} in state: {}", eventName, dsm.getCurrentStateName());
    }

    try {
        // Dispatch event to state machine.
        dsm.handleEvent(event);

        auto duration = std::chrono::steady_clock::now() - startTime;
        auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();

        spdlog::debug("Event processed in {} us", durationUs);
    }
    catch (const std::exception& e) {
        spdlog::error("Exception processing event {}: {}", getEventName(event), e.what());
        // Note: Following the design doc, we log and continue rather than exit.
    }
}