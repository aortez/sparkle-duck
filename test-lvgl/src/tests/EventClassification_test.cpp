#include <gtest/gtest.h>
#include "../Event.h"
#include "../EventTraits.h"
#include "../SharedSimState.h"
#include "../SynchronizedQueue.h"
#include <type_traits>
#include <thread>
#include <atomic>

// Note: Event types may not be in DirtSim namespace.
// Remove if compilation fails.

class EventClassificationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset any global state if needed.
    }
};

// ===== Static Type Tests =====

// Test that immediate events are correctly classified at compile time.
TEST_F(EventClassificationTest, ImmediateEventTraits) {
    // These should be immediate.
    EXPECT_TRUE(IsImmediateEvent<GetFPSCommand>::value);
    EXPECT_TRUE(IsImmediateEvent<GetSimStatsCommand>::value);
    
    // These should NOT be immediate.
    EXPECT_FALSE(IsImmediateEvent<StartSimulationCommand>::value);
    EXPECT_FALSE(IsImmediateEvent<ResetSimulationCommand>::value);
    EXPECT_FALSE(IsImmediateEvent<MouseDownEvent>::value);
    EXPECT_FALSE(IsImmediateEvent<SetTimescaleCommand>::value);
    EXPECT_FALSE(IsImmediateEvent<PauseCommand>::value);
    EXPECT_FALSE(IsImmediateEvent<ResumeCommand>::value);
}

// Test the runtime helper function.
TEST_F(EventClassificationTest, RuntimeEventClassification) {
    // Test immediate events.
    EXPECT_TRUE(isImmediateEvent(Event{GetFPSCommand{}}));
    EXPECT_TRUE(isImmediateEvent(Event{GetSimStatsCommand{}}));
    
    // Test queued events.
    EXPECT_FALSE(isImmediateEvent(Event{StartSimulationCommand{}}));
    EXPECT_FALSE(isImmediateEvent(Event{AdvanceSimulationCommand{}}));
    EXPECT_FALSE(isImmediateEvent(Event{MouseDownEvent{100, 200}}));
    EXPECT_FALSE(isImmediateEvent(Event{SelectMaterialCommand{MaterialType::WATER}}));
    EXPECT_FALSE(isImmediateEvent(Event{PauseCommand{}}));
    EXPECT_FALSE(isImmediateEvent(Event{ResumeCommand{}}));
}

// ===== Event Name Tests =====

TEST_F(EventClassificationTest, EventNames) {
    // Test that events have proper names.
    EXPECT_STREQ(GetFPSCommand::name(), "GetFPSCommand");
    EXPECT_STREQ(PauseCommand::name(), "PauseCommand");
    EXPECT_STREQ(MouseDownEvent::name(), "MouseDownEvent");
    EXPECT_STREQ(SetTimescaleCommand::name(), "SetTimescaleCommand");
    
    // Test getEventName helper.
    EXPECT_EQ(getEventName(Event{GetFPSCommand{}}), "GetFPSCommand");
    EXPECT_EQ(getEventName(Event{StartSimulationCommand{}}), "StartSimulationCommand");
}

// ===== Event Routing Logic Tests =====

TEST_F(EventClassificationTest, EventRoutingLogic) {
    // Test the routing logic without needing full EventRouter.
    std::vector<std::string> immediateEvents;
    std::vector<std::string> queuedEvents;
    
    auto routeEvent = [&](const Event& event) {
        std::visit([&](auto&& e) {
            using T = std::decay_t<decltype(e)>;
            
            if constexpr (IsImmediateEvent<T>::value) {
                immediateEvents.push_back(getEventName(event));
            } else {
                queuedEvents.push_back(getEventName(event));
            }
        }, event);
    };
    
    // Route various events.
    routeEvent(Event{GetFPSCommand{}});
    routeEvent(Event{PauseCommand{}});
    routeEvent(Event{StartSimulationCommand{}});
    routeEvent(Event{MouseDownEvent{50, 50}});
    routeEvent(Event{ResumeCommand{}});
    routeEvent(Event{SetTimescaleCommand{0.5}});
    
    // Check routing results.
    EXPECT_EQ(immediateEvents.size(), 1);
    EXPECT_EQ(queuedEvents.size(), 5);
    
    // Verify correct classification.
    EXPECT_EQ(immediateEvents[0], "GetFPSCommand");
    
    EXPECT_EQ(queuedEvents[0], "PauseCommand");
    EXPECT_EQ(queuedEvents[1], "StartSimulationCommand");
    EXPECT_EQ(queuedEvents[2], "MouseDownEvent");
    EXPECT_EQ(queuedEvents[3], "ResumeCommand");
    EXPECT_EQ(queuedEvents[4], "SetTimescaleCommand");
}

// ===== Event Data Tests =====

TEST_F(EventClassificationTest, EventDataIntegrity) {
    // Test that event data is preserved.
    MouseDownEvent mouseEvent{123, 456};
    Event wrappedEvent{mouseEvent};
    
    auto* extracted = std::get_if<MouseDownEvent>(&wrappedEvent);
    ASSERT_NE(extracted, nullptr);
    EXPECT_EQ(extracted->pixelX, 123);
    EXPECT_EQ(extracted->pixelY, 456);
    
    // Test material command.
    SelectMaterialCommand matCmd{MaterialType::SAND};
    Event wrappedMatCmd{matCmd};
    
    auto* extractedMat = std::get_if<SelectMaterialCommand>(&wrappedMatCmd);
    ASSERT_NE(extractedMat, nullptr);
    EXPECT_EQ(extractedMat->material, MaterialType::SAND);
}

// ===== Shared State Tests =====

TEST_F(EventClassificationTest, SharedStateAccess) {
    SharedSimState state;
    
    // Test atomic operations.
    EXPECT_FALSE(state.getShouldExit());
    state.setShouldExit(true);
    EXPECT_TRUE(state.getShouldExit());
    
    EXPECT_FALSE(state.getIsPaused());
    state.setIsPaused(true);
    EXPECT_TRUE(state.getIsPaused());
    
    // Test material selection.
    state.setSelectedMaterial(MaterialType::WATER);
    EXPECT_EQ(state.getSelectedMaterial(), MaterialType::WATER);
    
    // Test physics params.
    auto params = state.getPhysicsParams();
    params.gravityEnabled = true;
    params.elasticity = 0.75;
    state.updatePhysicsParams(params);
    
    auto newParams = state.getPhysicsParams();
    EXPECT_TRUE(newParams.gravityEnabled);
    EXPECT_FLOAT_EQ(newParams.elasticity, 0.75);
}

// ===== Thread Safety Tests =====

TEST_F(EventClassificationTest, EventQueueThreadSafety) {
    SynchronizedQueue<Event> queue;
    const int numEvents = 1000;
    
    // Producer thread.
    std::thread producer([&queue, numEvents]() {
        for (int i = 0; i < numEvents; ++i) {
            if (i % 2 == 0) {
                queue.push(Event{MouseDownEvent{i, i}});
            } else {
                queue.push(Event{PauseCommand{}});
            }
        }
    });
    
    // Consumer thread.
    std::atomic<int> consumed{0};
    std::thread consumer([&queue, &consumed, numEvents]() {
        while (consumed < numEvents) {
            auto eventOpt = queue.tryPop();
            if (eventOpt) {
                consumed++;
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_EQ(consumed.load(), numEvents);
    EXPECT_TRUE(queue.empty());
}

// ===== Event Variant Tests =====

TEST_F(EventClassificationTest, EventVariantSize) {
    // Ensure Event variant isn't too large.
    constexpr size_t eventSize = sizeof(Event);
    constexpr size_t maxAcceptableSize = 64; // Reasonable cache line size.
    
    EXPECT_LE(eventSize, maxAcceptableSize) 
        << "Event variant is too large: " << eventSize << " bytes";
}

TEST_F(EventClassificationTest, EventVisitor) {
    int immediateCount = 0;
    int queuedCount = 0;
    
    auto visitor = [&immediateCount, &queuedCount](const auto& event) {
        using T = std::decay_t<decltype(event)>;
        if constexpr (IsImmediateEvent<T>::value) {
            immediateCount++;
        } else {
            queuedCount++;
        }
    };
    
    // Visit various events.
    std::visit(visitor, Event{GetFPSCommand{}});
    std::visit(visitor, Event{PauseCommand{}});
    std::visit(visitor, Event{StartSimulationCommand{}});
    std::visit(visitor, Event{MouseDownEvent{0, 0}});
    
    EXPECT_EQ(immediateCount, 1);
    EXPECT_EQ(queuedCount, 3);
}