#include <gtest/gtest.h>
#include "../EventRouter.h"
#include "../DirtSimStateMachine.h"
#include "../SharedSimState.h"
#include "../Event.h"
#include <thread>
#include <chrono>

namespace {

using namespace DirtSim;

class DualPathTest : public ::testing::Test {
protected:
    std::unique_ptr<DirtSimStateMachine> stateMachine;
    std::unique_ptr<EventRouter> router;
    SharedSimState* sharedState = nullptr;
    
    void SetUp() override {
        stateMachine = std::make_unique<DirtSimStateMachine>(nullptr);
        sharedState = &stateMachine->getSharedState();
        router = std::make_unique<EventRouter>(*stateMachine, *sharedState, 
                                              stateMachine->eventProcessor.getEventQueue());
    }
};

TEST_F(DualPathTest, ImmediateEventProcessedImmediatelyWhenPushDisabled) {
    // Ensure push updates are disabled.

    // Send GetFPSCommand.
    GetFPSCommand cmd;
    router->routeEvent(cmd);
    
    // Event should have been processed immediately.
    // Check that it doesn't go to the queue.
    EXPECT_EQ(stateMachine->eventProcessor.getEventQueue().size(), 0);
}

TEST_F(DualPathTest, ImmediateEventQueuedWhenPushEnabled) {
    // Enable push updates.

    // Send GetFPSCommand.
    GetFPSCommand cmd;
    router->routeEvent(cmd);
    
    // Give a moment for event to be queued.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Event should have been queued instead of processed immediately.
    EXPECT_GT(stateMachine->eventProcessor.getEventQueue().size(), 0);
}

TEST_F(DualPathTest, AllToggleCommandsRoutedThroughPushWhenEnabled) {

    // Test all toggle commands.
    std::vector<Event> toggleEvents = {
        ToggleDebugCommand{},
        ToggleForceCommand{},
        ToggleCohesionCommand{},
        ToggleAdhesionCommand{},
        ToggleTimeHistoryCommand{}
    };
    
    size_t initialQueueSize = stateMachine->eventProcessor.getEventQueue().size();
    
    for (const auto& event : toggleEvents) {
        router->routeEvent(event);
    }
    
    // Give a moment for events to be queued.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // All events should be queued.
    EXPECT_EQ(stateMachine->eventProcessor.getEventQueue().size(), 
              initialQueueSize + toggleEvents.size());
}

TEST_F(DualPathTest, GetCommandsRoutedThroughPushWhenEnabled) {

    std::vector<Event> getCommands = {
        GetFPSCommand{},
        GetSimStatsCommand{}
    };
    
    size_t initialQueueSize = stateMachine->eventProcessor.getEventQueue().size();
    
    for (const auto& event : getCommands) {
        router->routeEvent(event);
    }
    
    // Give a moment for events to be queued.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // All events should be queued.
    EXPECT_EQ(stateMachine->eventProcessor.getEventQueue().size(), 
              initialQueueSize + getCommands.size());
}

TEST_F(DualPathTest, PrintAsciiDiagramCommandRoutedThroughPush) {

    PrintAsciiDiagramCommand cmd;
    router->routeEvent(cmd);
    
    // Give a moment for event to be queued.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Event should be queued.
    EXPECT_GT(stateMachine->eventProcessor.getEventQueue().size(), 0);
}

TEST_F(DualPathTest, NonImmediateEventsAlwaysQueued) {
    // Test with push disabled.

    PauseCommand pauseCmd;
    router->routeEvent(pauseCmd);
    
    // Should be queued regardless of push state.
    EXPECT_GT(stateMachine->eventProcessor.getEventQueue().size(), 0);
    
    // Clear queue.
    while (!stateMachine->eventProcessor.getEventQueue().empty()) {
        stateMachine->eventProcessor.getEventQueue().pop();
    }
    
    // Test with push enabled.

    ResumeCommand resumeCmd;
    router->routeEvent(resumeCmd);
    
    // Should still be queued.
    EXPECT_GT(stateMachine->eventProcessor.getEventQueue().size(), 0);
}

TEST_F(DualPathTest, ToggleDebugCommandUpdatesDebugFlag) {
    // Enable push updates to route through state machine.

    // Transition to SimRunning state.
    stateMachine->handleEvent(InitCompleteEvent{});
    stateMachine->handleEvent(StartSimulationCommand{});
    
    // Check initial state from world (source of truth).
    auto* world = stateMachine->getSimulationManager()->getWorld();
    ASSERT_NE(world, nullptr);
    bool initialDebugState = world->isDebugDrawEnabled();

    // Send toggle command.
    router->routeEvent(ToggleDebugCommand{});

    // Process the queued event.
    stateMachine->eventProcessor.processEventsFromQueue(*stateMachine);

    // Debug flag in world should be toggled.
    EXPECT_NE(world->isDebugDrawEnabled(), initialDebugState);
}

TEST_F(DualPathTest, ToggleCommandsGeneratePushUpdates) {
    // Enable push updates.

    // Transition to SimRunning state.
    stateMachine->handleEvent(InitCompleteEvent{});
    stateMachine->handleEvent(StartSimulationCommand{});
    
    // Clear any existing updates.
    while (sharedState->hasUIUpdatePending()) {
        sharedState->popUIUpdate();
    }
    
    // Send toggle command.
    router->routeEvent(ToggleForceCommand{});
    
    // Process the queued event.
    stateMachine->eventProcessor.processEventsFromQueue(*stateMachine);
    
    // Should have generated a push update.
    EXPECT_TRUE(sharedState->hasUIUpdatePending());
    
    // Pop the update and check dirty flags.
    auto update = sharedState->popUIUpdate();
    EXPECT_TRUE(update.has_value());
    EXPECT_TRUE(update->dirty.physicsParams);
}

TEST_F(DualPathTest, GetFPSCommandGeneratesFPSDirtyFlag) {
    // Enable push updates.

    // Transition to SimRunning state.
    stateMachine->handleEvent(InitCompleteEvent{});
    stateMachine->handleEvent(StartSimulationCommand{});
    
    // Clear any existing updates.
    while (sharedState->hasUIUpdatePending()) {
        sharedState->popUIUpdate();
    }
    
    // Send GetFPS command.
    router->routeEvent(GetFPSCommand{});
    
    // Process the queued event.
    stateMachine->eventProcessor.processEventsFromQueue(*stateMachine);
    
    // Should have generated a push update with FPS dirty flag.
    EXPECT_TRUE(sharedState->hasUIUpdatePending());
    
    auto update = sharedState->popUIUpdate();
    EXPECT_TRUE(update.has_value());
    EXPECT_TRUE(update->dirty.fps);
}

TEST_F(DualPathTest, GetSimStatsCommandGeneratesStatsDirtyFlags) {
    // Enable push updates.

    // Transition to SimRunning state.
    stateMachine->handleEvent(InitCompleteEvent{});
    stateMachine->handleEvent(StartSimulationCommand{});
    
    // Clear any existing updates.
    while (sharedState->hasUIUpdatePending()) {
        sharedState->popUIUpdate();
    }
    
    // Send GetSimStats command.
    router->routeEvent(GetSimStatsCommand{});
    
    // Process the queued event.
    stateMachine->eventProcessor.processEventsFromQueue(*stateMachine);
    
    // Should have generated a push update with stats dirty flags.
    EXPECT_TRUE(sharedState->hasUIUpdatePending());
    
    auto update = sharedState->popUIUpdate();
    EXPECT_TRUE(update.has_value());
    EXPECT_TRUE(update->dirty.stats);
}

} // namespace.