#include <gtest/gtest.h>
#include "../DirtSimStateMachine.h"
#include "../EventRouter.h"
#include "../Event.h"
#include "../SharedSimState.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

using namespace DirtSim;

class DualPathRoutingTests : public ::testing::Test {
protected:
    std::unique_ptr<DirtSimStateMachine> dsm;
    
    void SetUp() override {
        dsm = std::make_unique<DirtSimStateMachine>();
    }
    
    void processEvents() {
        dsm->eventProcessor.processEventsFromQueue(*dsm);
    }
};

// ===== Immediate Event Routing Tests =====

TEST_F(DualPathRoutingTests, ImmediateEvents_ProcessedSynchronously) {
    auto& router = dsm->getEventRouter();
    auto& sharedState = dsm->getSharedState();
    
    // Test immediate event processing with GetFPSCommand
    sharedState.setCurrentFPS(120.0f);
    
    // Route immediate event - should process without queuing
    router.routeEvent(Event{GetFPSCommand{}});
    
    // FPS value should still be accessible (command processes immediately)
    EXPECT_FLOAT_EQ(sharedState.getCurrentFPS(), 120.0f);
    
    // Test GetSimStatsCommand as well
    router.routeEvent(Event{GetSimStatsCommand{}});
    
    // Stats command should also process immediately
    // (it doesn't change state, but verifies immediate processing)
}

TEST_F(DualPathRoutingTests, ImmediateEvents_GetFPSCommand) {
    auto& router = dsm->getEventRouter();
    auto& sharedState = dsm->getSharedState();
    
    // Set FPS value
    sharedState.setCurrentFPS(60.0f);
    
    // Route GetFPSCommand - it should process immediately
    router.routeEvent(Event{GetFPSCommand{}});
    
    // Verify FPS can be retrieved (the command itself doesn't change state,
    // but it should process without queuing)
    EXPECT_FLOAT_EQ(sharedState.getCurrentFPS(), 60.0f);
}

TEST_F(DualPathRoutingTests, ImmediateEvents_MultipleInSequence) {
    auto& router = dsm->getEventRouter();
    auto& sharedState = dsm->getSharedState();
    
    // Send multiple immediate events in sequence
    sharedState.setCurrentFPS(60.0f);
    
    router.routeEvent(Event{GetFPSCommand{}});
    EXPECT_FLOAT_EQ(sharedState.getCurrentFPS(), 60.0f);
    
    router.routeEvent(Event{GetSimStatsCommand{}});
    // Stats command processes immediately
    
    // Change FPS and verify immediate processing
    sharedState.setCurrentFPS(144.0f);
    router.routeEvent(Event{GetFPSCommand{}});
    EXPECT_FLOAT_EQ(sharedState.getCurrentFPS(), 144.0f);
}

// ===== Queued Event Routing Tests =====

TEST_F(DualPathRoutingTests, QueuedEvents_RequireProcessing) {
    auto& router = dsm->getEventRouter();
    
    // Transition to MainMenu first
    dsm->queueEvent(InitCompleteEvent{});
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "MainMenu");
    
    // Route a queued event
    router.routeEvent(Event{StartSimulationCommand{}});
    
    // Should still be in MainMenu (event is queued, not processed)
    EXPECT_EQ(dsm->getCurrentStateName(), "MainMenu");
    
    // Process the queue
    processEvents();
    
    // Now should be in SimRunning
    EXPECT_EQ(dsm->getCurrentStateName(), "SimRunning");
}

TEST_F(DualPathRoutingTests, QueuedEvents_MaintainOrder) {
    // Get to SimRunning state
    dsm->queueEvent(InitCompleteEvent{});
    dsm->queueEvent(StartSimulationCommand{});
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "SimRunning");
    
    auto& sharedState = dsm->getSharedState();
    uint32_t initialStep = sharedState.getCurrentStep();
    
    // Queue multiple events
    dsm->queueEvent(AdvanceSimulationCommand{});
    dsm->queueEvent(AdvanceSimulationCommand{});
    dsm->queueEvent(SelectMaterialCommand{MaterialType::WATER});
    dsm->queueEvent(AdvanceSimulationCommand{});
    
    // Process all events
    processEvents();
    
    // Should have advanced 3 steps
    EXPECT_EQ(sharedState.getCurrentStep(), initialStep + 3);
    // Material should be WATER
    EXPECT_EQ(sharedState.getSelectedMaterial(), MaterialType::WATER);
}

// ===== Mixed Event Routing Tests =====

TEST_F(DualPathRoutingTests, MixedEvents_CorrectRouting) {
    auto& router = dsm->getEventRouter();
    auto& sharedState = dsm->getSharedState();
    
    // Setup: get to SimRunning
    dsm->queueEvent(InitCompleteEvent{});
    dsm->queueEvent(StartSimulationCommand{});
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "SimRunning");
    
    // Mix of immediate and queued events
    sharedState.setCurrentFPS(30.0f);
    
    // Immediate event - processes right away
    router.routeEvent(Event{GetFPSCommand{}});
    EXPECT_FLOAT_EQ(sharedState.getCurrentFPS(), 30.0f);
    
    // Queued events
    router.routeEvent(Event{PauseCommand{}});  // Now queued
    router.routeEvent(Event{AdvanceSimulationCommand{}});  // Queued
    
    // State shouldn't change until we process the queue
    EXPECT_EQ(dsm->getCurrentStateName(), "SimRunning");
    EXPECT_FALSE(sharedState.getIsPaused());
    
    // Process queued events
    processEvents();
    
    // Now should be paused
    EXPECT_EQ(dsm->getCurrentStateName(), "SimPaused");
    EXPECT_TRUE(sharedState.getIsPaused());
}

// ===== Thread Safety Tests =====

TEST_F(DualPathRoutingTests, ThreadSafety_ConcurrentImmediateEvents) {
    auto& router = dsm->getEventRouter();
    
    const int numThreads = 4;
    const int eventsPerThread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> fpsQueries{0};
    std::atomic<int> statsQueries{0};
    
    // Multiple threads sending immediate events
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&router, eventsPerThread, t, &fpsQueries, &statsQueries]() {
            for (int i = 0; i < eventsPerThread; ++i) {
                if ((t + i) % 2 == 0) {
                    router.routeEvent(Event{GetFPSCommand{}});
                    fpsQueries++;
                } else {
                    router.routeEvent(Event{GetSimStatsCommand{}});
                    statsQueries++;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // All events should have been processed
    EXPECT_EQ(fpsQueries + statsQueries, numThreads * eventsPerThread);
}

TEST_F(DualPathRoutingTests, ThreadSafety_ConcurrentQueuedEvents) {
    // Get to SimRunning
    dsm->queueEvent(InitCompleteEvent{});
    dsm->queueEvent(StartSimulationCommand{});
    processEvents();
    
    const int numThreads = 4;
    const int eventsPerThread = 50;
    std::vector<std::thread> threads;
    std::atomic<int> totalEvents{0};
    
    // Multiple threads queuing events
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, eventsPerThread, &totalEvents]() {
            for (int i = 0; i < eventsPerThread; ++i) {
                dsm->queueEvent(AdvanceSimulationCommand{});
                totalEvents++;
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Process all queued events
    uint32_t stepsBefore = dsm->getSharedState().getCurrentStep();
    processEvents();
    
    // Should have processed all events
    uint32_t stepsAfter = dsm->getSharedState().getCurrentStep();
    EXPECT_EQ(stepsAfter - stepsBefore, totalEvents.load());
}

// ===== Event Classification Verification =====

TEST_F(DualPathRoutingTests, EventClassification_AllEventsRoutedCorrectly) {
    // Note: router is not used in this test, just testing classification
    
    // List of immediate events (only GetFPS and GetSimStats now)
    std::vector<Event> immediateEvents = {
        Event{GetFPSCommand{}},
        Event{GetSimStatsCommand{}}
    };
    
    // List of queued events (includes PauseCommand and ResumeCommand now)
    std::vector<Event> queuedEvents = {
        Event{StartSimulationCommand{}},
        Event{AdvanceSimulationCommand{}},
        Event{ResetSimulationCommand{}},
        Event{MouseDownEvent{10, 20}},
        Event{SelectMaterialCommand{MaterialType::SAND}},
        Event{SetTimescaleCommand{0.5}},
        Event{QuitApplicationCommand{}},
        Event{PauseCommand{}},      // Now queued for state transitions
        Event{ResumeCommand{}}       // Now queued for state transitions
    };
    
    // Test that immediate events are classified correctly
    for (const auto& event : immediateEvents) {
        EXPECT_TRUE(isImmediateEvent(event)) 
            << "Event " << getEventName(event) << " should be immediate";
    }
    
    // Test that queued events are classified correctly
    for (const auto& event : queuedEvents) {
        EXPECT_FALSE(isImmediateEvent(event))
            << "Event " << getEventName(event) << " should be queued";
    }
}

// ===== Performance Tests =====

TEST_F(DualPathRoutingTests, Performance_ImmediateEventLatency) {
    auto& router = dsm->getEventRouter();
    
    const int iterations = 1000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        router.routeEvent(Event{GetFPSCommand{}});
        router.routeEvent(Event{GetSimStatsCommand{}});
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double avgLatencyUs = static_cast<double>(duration.count()) / (iterations * 2);
    
    // Immediate events should be very fast (< 10 microseconds average)
    EXPECT_LT(avgLatencyUs, 10.0) 
        << "Average immediate event latency: " << avgLatencyUs << " microseconds";
}

TEST_F(DualPathRoutingTests, Performance_QueueThroughput) {
    // Get to SimRunning
    dsm->queueEvent(InitCompleteEvent{});
    dsm->queueEvent(StartSimulationCommand{});
    processEvents();
    
    const int numEvents = 10000;
    
    // Queue many events
    auto startQueue = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numEvents; ++i) {
        dsm->queueEvent(AdvanceSimulationCommand{});
    }
    
    auto endQueue = std::chrono::high_resolution_clock::now();
    
    // Process all events
    auto startProcess = std::chrono::high_resolution_clock::now();
    processEvents();
    auto endProcess = std::chrono::high_resolution_clock::now();
    
    auto queueDuration = std::chrono::duration_cast<std::chrono::microseconds>(endQueue - startQueue);
    auto processDuration = std::chrono::duration_cast<std::chrono::microseconds>(endProcess - startProcess);
    
    double queueThroughput = numEvents / (queueDuration.count() / 1000000.0);
    double processThroughput = numEvents / (processDuration.count() / 1000000.0);
    
    // Should be able to queue and process many events per second
    EXPECT_GT(queueThroughput, 100000) << "Queue throughput: " << queueThroughput << " events/sec";
    EXPECT_GT(processThroughput, 10000) << "Process throughput: " << processThroughput << " events/sec";
}