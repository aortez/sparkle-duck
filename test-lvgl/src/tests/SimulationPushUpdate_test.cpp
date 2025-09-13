#include "../DirtSimStateMachine.h"
#include "../Event.h"
#include "../SharedSimState.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

using namespace DirtSim;

class SimulationPushUpdateTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create state machine without display for testing.
        dsm = std::make_unique<DirtSimStateMachine>(nullptr);
    }

    void TearDown() override {
        dsm.reset();
    }

    std::unique_ptr<DirtSimStateMachine> dsm;
};

TEST_F(SimulationPushUpdateTest, PushDisabledByDefault) {
    // Feature should be disabled by default.
    EXPECT_FALSE(dsm->getSharedState().isPushUpdatesEnabled());
    
    // Queue some events to advance to SimRunning.
    dsm->queueEvent(InitCompleteEvent{});
    dsm->queueEvent(StartSimulationCommand{});
    
    // Process events.
    for (int i = 0; i < 5; ++i) {
        dsm->eventProcessor.processEventsFromQueue(*dsm);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Get initial metrics.
    auto metrics = dsm->getSharedState().getUIUpdateMetrics();
    size_t initialPushCount = metrics.pushCount;
    
    // Advance simulation.
    dsm->queueEvent(AdvanceSimulationCommand{});
    for (int i = 0; i < 5; ++i) {
        dsm->eventProcessor.processEventsFromQueue(*dsm);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Should be no new pushes when disabled.
    metrics = dsm->getSharedState().getUIUpdateMetrics();
    EXPECT_EQ(initialPushCount, metrics.pushCount);
}

TEST_F(SimulationPushUpdateTest, PushOnSimulationAdvance) {
    // Enable push updates.
    dsm->getSharedState().enablePushUpdates(true);
    EXPECT_TRUE(dsm->getSharedState().isPushUpdatesEnabled());
    
    // Queue events to get to SimRunning.
    dsm->queueEvent(InitCompleteEvent{});
    dsm->queueEvent(StartSimulationCommand{});
    
    // Process events.
    for (int i = 0; i < 5; ++i) {
        dsm->eventProcessor.processEventsFromQueue(*dsm);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Get initial metrics.
    auto metrics = dsm->getSharedState().getUIUpdateMetrics();
    size_t initialPushCount = metrics.pushCount;
    
    // Advance simulation multiple times.
    const int advanceCount = 5;
    for (int i = 0; i < advanceCount; ++i) {
        dsm->queueEvent(AdvanceSimulationCommand{});
        dsm->eventProcessor.processEventsFromQueue(*dsm);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Should have pushed updates.
    metrics = dsm->getSharedState().getUIUpdateMetrics();
    EXPECT_GT(metrics.pushCount, initialPushCount);
    
    // Pop an update and verify content.
    auto updateOpt = dsm->getSharedState().popUIUpdate();
    EXPECT_TRUE(updateOpt.has_value());
    if (updateOpt) {
        const UIUpdateEvent& update = *updateOpt;
        EXPECT_EQ(60u, update.fps);  // Hardcoded in SimRunning.
        EXPECT_GT(update.stepCount, 0u);
    }
}

TEST_F(SimulationPushUpdateTest, PushOnStateTransition) {
    // Enable push updates.
    dsm->getSharedState().enablePushUpdates(true);
    
    // Get to SimRunning first.
    dsm->queueEvent(InitCompleteEvent{});
    dsm->queueEvent(StartSimulationCommand{});
    
    // Process events.
    for (int i = 0; i < 5; ++i) {
        dsm->eventProcessor.processEventsFromQueue(*dsm);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Get metrics before pause.
    auto metrics = dsm->getSharedState().getUIUpdateMetrics();
    size_t pushCountBefore = metrics.pushCount;
    
    // Transition to paused (state change).
    dsm->queueEvent(PauseCommand{});
    for (int i = 0; i < 3; ++i) {
        dsm->eventProcessor.processEventsFromQueue(*dsm);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Should have pushed on state transition.
    metrics = dsm->getSharedState().getUIUpdateMetrics();
    EXPECT_GT(metrics.pushCount, pushCountBefore);
    
    // Verify pause state is reflected.
    auto updateOpt = dsm->getSharedState().popUIUpdate();
    if (updateOpt) {
        const UIUpdateEvent& update = *updateOpt;
        EXPECT_TRUE(update.isPaused);
        EXPECT_TRUE(update.dirty.uiState);  // Should be marked dirty.
    }
}

TEST_F(SimulationPushUpdateTest, PushOnPausedAdvance) {
    // Enable push updates.
    dsm->getSharedState().enablePushUpdates(true);
    
    // Get to SimPaused state.
    dsm->queueEvent(InitCompleteEvent{});
    dsm->queueEvent(StartSimulationCommand{});
    
    // Process to get to SimRunning.
    for (int i = 0; i < 5; ++i) {
        dsm->eventProcessor.processEventsFromQueue(*dsm);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Now pause.
    dsm->queueEvent(PauseCommand{});
    for (int i = 0; i < 3; ++i) {
        dsm->eventProcessor.processEventsFromQueue(*dsm);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Clear any pending updates.
    while (dsm->getSharedState().popUIUpdate()) {}
    
    // Get metrics.
    auto metrics = dsm->getSharedState().getUIUpdateMetrics();
    size_t pushCountBefore = metrics.pushCount;
    
    // Single step advance while paused.
    dsm->queueEvent(AdvanceSimulationCommand{});
    dsm->eventProcessor.processEventsFromQueue(*dsm);
    
    // Should have pushed update.
    metrics = dsm->getSharedState().getUIUpdateMetrics();
    EXPECT_EQ(pushCountBefore + 1, metrics.pushCount);
    
    // Verify update.
    auto updateOpt = dsm->getSharedState().popUIUpdate();
    EXPECT_TRUE(updateOpt.has_value());
    if (updateOpt) {
        EXPECT_TRUE(updateOpt->isPaused);  // Still paused.
    }
}

TEST_F(SimulationPushUpdateTest, LatestUpdateWins) {
    // Enable push updates.
    dsm->getSharedState().enablePushUpdates(true);
    
    // Get to SimRunning.
    dsm->queueEvent(InitCompleteEvent{});
    dsm->queueEvent(StartSimulationCommand{});
    
    // Process events.
    for (int i = 0; i < 5; ++i) {
        dsm->eventProcessor.processEventsFromQueue(*dsm);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Clear queue.
    while (dsm->getSharedState().popUIUpdate()) {}
    
    // Get initial step count.
    size_t initialStep = dsm->getSharedState().getCurrentStep();
    
    // Queue multiple advances rapidly.
    for (int i = 0; i < 10; ++i) {
        dsm->queueEvent(AdvanceSimulationCommand{});
    }
    
    // Process all.
    for (int i = 0; i < 15; ++i) {
        dsm->eventProcessor.processEventsFromQueue(*dsm);
    }
    
    // Should only have one update (latest).
    auto updateOpt = dsm->getSharedState().popUIUpdate();
    EXPECT_TRUE(updateOpt.has_value());
    EXPECT_FALSE(dsm->getSharedState().popUIUpdate().has_value());  // No second update.
    
    // The update should have the latest step count.
    if (updateOpt) {
        EXPECT_GT(updateOpt->stepCount, initialStep);
    }
    
    // Check drop count increased.
    auto metrics = dsm->getSharedState().getUIUpdateMetrics();
    EXPECT_GT(metrics.dropCount, 0u);
}