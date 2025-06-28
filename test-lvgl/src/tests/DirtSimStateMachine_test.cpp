#include <gtest/gtest.h>
#include "../DirtSimStateMachine.h"
#include "../states/State.h"
#include "../Event.h"
#include <thread>
#include <chrono>

using namespace DirtSim;

class DirtSimStateMachineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Each test gets a fresh state machine.
        // Note: LVGL is not initialized in unit tests, so UI creation will be skipped.
    }
};

// Test initial state.
TEST_F(DirtSimStateMachineTest, InitialState) {
    DirtSimStateMachine dsm;
    
    // Should start in Startup state.
    EXPECT_EQ(dsm.getCurrentStateName(), "Startup");
    EXPECT_FALSE(dsm.shouldExit());
}

// Test state transitions.
TEST_F(DirtSimStateMachineTest, BasicStateTransitions) {
    DirtSimStateMachine dsm;
    
    // Send InitCompleteEvent to transition from Startup to MainMenu.
    dsm.queueEvent(InitCompleteEvent{});
    
    // Process the event from the queue.
    // Note: In real app, this would be done by the event loop.
    dsm.eventProcessor.processEventsFromQueue(dsm);
    
    EXPECT_EQ(dsm.getCurrentStateName(), "MainMenu");
}

// Test event routing.
TEST_F(DirtSimStateMachineTest, EventRouting) {
    DirtSimStateMachine dsm;
    auto& router = dsm.getEventRouter();
    
    // First, get to a state where we can test pause/resume.
    dsm.queueEvent(InitCompleteEvent{});
    dsm.eventProcessor.processEventsFromQueue(dsm);
    dsm.queueEvent(StartSimulationCommand{});
    dsm.eventProcessor.processEventsFromQueue(dsm);
    EXPECT_EQ(dsm.getCurrentStateName(), "SimRunning");
    
    // Test queued event routing (PauseCommand is now queued, not immediate).
    router.routeEvent(Event{PauseCommand{}});
    
    // Process the queued event.
    dsm.eventProcessor.processEventsFromQueue(dsm);
    
    // Check that we transitioned to SimPaused.
    EXPECT_EQ(dsm.getCurrentStateName(), "SimPaused");
    
    // Test resume.
    router.routeEvent(Event{ResumeCommand{}});
    dsm.eventProcessor.processEventsFromQueue(dsm);
    EXPECT_EQ(dsm.getCurrentStateName(), "SimRunning");
}

// Test quit command.
TEST_F(DirtSimStateMachineTest, QuitCommand) {
    DirtSimStateMachine dsm;
    
    // Send quit command.
    dsm.queueEvent(QuitApplicationCommand{});
    dsm.eventProcessor.processEventsFromQueue(dsm);
    
    // Should transition to Shutdown and set exit flag.
    EXPECT_EQ(dsm.getCurrentStateName(), "Shutdown");
    EXPECT_TRUE(dsm.shouldExit());
}

// Test shared state.
TEST_F(DirtSimStateMachineTest, SharedStateAccess) {
    DirtSimStateMachine dsm;
    auto& sharedState = dsm.getSharedState();
    
    // Test material selection.
    sharedState.setSelectedMaterial(MaterialType::WATER);
    EXPECT_EQ(sharedState.getSelectedMaterial(), MaterialType::WATER);
    
    // Test FPS setting.
    sharedState.setCurrentFPS(30.0f);
    EXPECT_FLOAT_EQ(sharedState.getCurrentFPS(), 30.0f);
    
    // Test step counter.
    sharedState.setCurrentStep(100);
    EXPECT_EQ(sharedState.getCurrentStep(), 100);
}

// Test simulation state transitions.
TEST_F(DirtSimStateMachineTest, SimulationStateFlow) {
    DirtSimStateMachine dsm;
    
    // Go to MainMenu.
    dsm.queueEvent(InitCompleteEvent{});
    dsm.eventProcessor.processEventsFromQueue(dsm);
    EXPECT_EQ(dsm.getCurrentStateName(), "MainMenu");
    
    // Start simulation.
    dsm.queueEvent(StartSimulationCommand{});
    dsm.eventProcessor.processEventsFromQueue(dsm);
    EXPECT_EQ(dsm.getCurrentStateName(), "SimRunning");
    
    // Pause simulation.
    dsm.queueEvent(PauseCommand{});
    dsm.eventProcessor.processEventsFromQueue(dsm);
    EXPECT_EQ(dsm.getCurrentStateName(), "SimPaused");
    
    // Resume simulation.
    dsm.queueEvent(ResumeCommand{});
    dsm.eventProcessor.processEventsFromQueue(dsm);
    EXPECT_EQ(dsm.getCurrentStateName(), "SimRunning");
}

// Test event classification and routing.
TEST_F(DirtSimStateMachineTest, EventClassificationInStateMachine) {
    DirtSimStateMachine dsm;
    auto& router = dsm.getEventRouter();
    
    // First, get to MainMenu state.
    dsm.queueEvent(InitCompleteEvent{});
    dsm.eventProcessor.processEventsFromQueue(dsm);
    EXPECT_EQ(dsm.getCurrentStateName(), "MainMenu");
    
    // Test queued event (SelectMaterialCommand is queued).
    router.routeEvent(Event{SelectMaterialCommand{MaterialType::WATER}});
    // Need to process queued events.
    dsm.eventProcessor.processEventsFromQueue(dsm);
    EXPECT_EQ(dsm.getSharedState().getSelectedMaterial(), MaterialType::WATER);
    
    // Test queued events (PauseCommand is now queued).
    dsm.queueEvent(StartSimulationCommand{});
    dsm.eventProcessor.processEventsFromQueue(dsm);
    EXPECT_EQ(dsm.getCurrentStateName(), "SimRunning");
    
    // Queue pause command.
    router.routeEvent(Event{PauseCommand{}});
    // Before processing, we're still in SimRunning.
    EXPECT_EQ(dsm.getCurrentStateName(), "SimRunning");
    
    // Process the queued event.
    dsm.eventProcessor.processEventsFromQueue(dsm);
    // Now we should be paused.
    EXPECT_EQ(dsm.getCurrentStateName(), "SimPaused");
}

// Test state lifecycle methods.
TEST_F(DirtSimStateMachineTest, StateLifecycle) {
    // This test verifies that onEnter/onExit are called.
    // We can't directly test this without mocking, but we can.
    // verify side effects.
    
    DirtSimStateMachine dsm;
    
    // Transition to MainMenu (should call Startup::onExit and MainMenu::onEnter).
    dsm.queueEvent(InitCompleteEvent{});
    dsm.eventProcessor.processEventsFromQueue(dsm);
    
    // Verify world was created (side effect of Startup state).
    EXPECT_NE(dsm.world, nullptr);
    
    // Note: In a real test, we'd want to mock the UI creation.
    // to verify MainMenu::onEnter was called.
}

// Test invalid event in state.
TEST_F(DirtSimStateMachineTest, InvalidEventInState) {
    DirtSimStateMachine dsm;
    
    // In Startup state, try to send a simulation event.
    dsm.queueEvent(AdvanceSimulationCommand{});
    dsm.eventProcessor.processEventsFromQueue(dsm);
    
    // Should stay in Startup state.
    EXPECT_EQ(dsm.getCurrentStateName(), "Startup");
}

// Test multiple state transitions.
TEST_F(DirtSimStateMachineTest, MultipleTransitions) {
    DirtSimStateMachine dsm;
    
    // Startup -> MainMenu -> Config -> MainMenu.
    dsm.queueEvent(InitCompleteEvent{});
    dsm.eventProcessor.processEventsFromQueue(dsm);
    EXPECT_EQ(dsm.getCurrentStateName(), "MainMenu");
    
    dsm.queueEvent(OpenConfigCommand{});
    dsm.eventProcessor.processEventsFromQueue(dsm);
    EXPECT_EQ(dsm.getCurrentStateName(), "Config");
    
    // Config uses StartSimulationCommand to go back (hack in our implementation).
    dsm.queueEvent(StartSimulationCommand{});
    dsm.eventProcessor.processEventsFromQueue(dsm);
    EXPECT_EQ(dsm.getCurrentStateName(), "MainMenu");
}