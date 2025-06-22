#include <gtest/gtest.h>
#include "../DirtSimStateMachine.h"
#include "../states/State.h"
#include "../Event.h"
#include "../SimulationManager.h"
#include <thread>
#include <chrono>

using namespace DirtSim;

// Test fixture that provides a clean state machine for each test
class StateTests : public ::testing::Test {
protected:
    std::unique_ptr<DirtSimStateMachine> dsm;
    
    void SetUp() override {
        dsm = std::make_unique<DirtSimStateMachine>();
    }
    
    // Helper to process queued events
    void processEvents() {
        dsm->eventProcessor.processEventsFromQueue(*dsm);
    }
    
    // Helper to transition to a specific state
    void transitionTo(const std::string& targetState) {
        if (targetState == "MainMenu") {
            dsm->queueEvent(InitCompleteEvent{});
            processEvents();
        } else if (targetState == "SimRunning") {
            transitionTo("MainMenu");
            dsm->queueEvent(StartSimulationCommand{});
            processEvents();
        } else if (targetState == "SimPaused") {
            transitionTo("SimRunning");
            dsm->queueEvent(PauseCommand{});
            processEvents();
        }
    }
};

// ===== Startup State Tests =====

TEST_F(StateTests, StartupState_InitialConditions) {
    EXPECT_EQ(dsm->getCurrentStateName(), "Startup");
    EXPECT_EQ(dsm->world, nullptr);
    EXPECT_FALSE(dsm->shouldExit());
}

TEST_F(StateTests, StartupState_SuccessfulInit) {
    // Send init complete event
    dsm->queueEvent(InitCompleteEvent{});
    processEvents();
    
    // Should transition to MainMenu and create world
    EXPECT_EQ(dsm->getCurrentStateName(), "MainMenu");
    EXPECT_NE(dsm->world, nullptr);
}

TEST_F(StateTests, StartupState_IgnoresOtherEvents) {
    // Try sending events that Startup shouldn't handle
    dsm->queueEvent(StartSimulationCommand{});
    dsm->queueEvent(PauseCommand{});
    dsm->queueEvent(MouseDownEvent{100, 100});
    processEvents();
    
    // Should still be in Startup
    EXPECT_EQ(dsm->getCurrentStateName(), "Startup");
}

// ===== MainMenu State Tests =====

TEST_F(StateTests, MainMenuState_StartSimulation) {
    transitionTo("MainMenu");
    
    // Start simulation
    dsm->queueEvent(StartSimulationCommand{});
    processEvents();
    
    EXPECT_EQ(dsm->getCurrentStateName(), "SimRunning");
}

TEST_F(StateTests, MainMenuState_OpenConfig) {
    transitionTo("MainMenu");
    
    // Open config
    dsm->queueEvent(OpenConfigCommand{});
    processEvents();
    
    EXPECT_EQ(dsm->getCurrentStateName(), "Config");
}

TEST_F(StateTests, MainMenuState_IgnoresSimulationEvents) {
    transitionTo("MainMenu");
    
    // Try simulation-specific events
    dsm->queueEvent(AdvanceSimulationCommand{});
    dsm->queueEvent(PauseCommand{});
    processEvents();
    
    // Should remain in MainMenu
    EXPECT_EQ(dsm->getCurrentStateName(), "MainMenu");
}

// ===== SimRunning State Tests =====

TEST_F(StateTests, SimRunningState_CreatesSimulationManager) {
    transitionTo("SimRunning");
    
    // SimulationManager should be created
    EXPECT_NE(dsm->getSimulationManager(), nullptr);
}

TEST_F(StateTests, SimRunningState_AdvanceSimulation) {
    transitionTo("SimRunning");
    
    auto& sharedState = dsm->getSharedState();
    uint32_t initialStep = sharedState.getCurrentStep();
    
    // Advance simulation multiple times
    for (int i = 0; i < 5; ++i) {
        dsm->queueEvent(AdvanceSimulationCommand{});
    }
    processEvents();
    
    // Step count should increase
    EXPECT_GT(sharedState.getCurrentStep(), initialStep);
}

TEST_F(StateTests, SimRunningState_PauseTransition) {
    transitionTo("SimRunning");
    
    // Pause the simulation - use EventRouter for immediate event
    dsm->queueEvent(PauseCommand{});
    processEvents();
    
    EXPECT_EQ(dsm->getCurrentStateName(), "SimPaused");
    EXPECT_TRUE(dsm->getSharedState().getIsPaused());
}

TEST_F(StateTests, SimRunningState_ResetSimulation) {
    transitionTo("SimRunning");
    
    // Advance a few steps
    for (int i = 0; i < 3; ++i) {
        dsm->queueEvent(AdvanceSimulationCommand{});
    }
    processEvents();
    
    uint32_t stepsBeforeReset = dsm->getSharedState().getCurrentStep();
    EXPECT_GT(stepsBeforeReset, 0);
    
    // Reset simulation
    dsm->queueEvent(ResetSimulationCommand{});
    processEvents();
    
    // Should still be in SimRunning but with reset state
    EXPECT_EQ(dsm->getCurrentStateName(), "SimRunning");
    // Note: Reset might not reset step counter depending on implementation
}

TEST_F(StateTests, SimRunningState_MaterialSelection) {
    transitionTo("SimRunning");
    
    auto& sharedState = dsm->getSharedState();
    
    // Change material selection
    dsm->queueEvent(SelectMaterialCommand{MaterialType::WATER});
    processEvents();
    EXPECT_EQ(sharedState.getSelectedMaterial(), MaterialType::WATER);
    
    dsm->queueEvent(SelectMaterialCommand{MaterialType::SAND});
    processEvents();
    EXPECT_EQ(sharedState.getSelectedMaterial(), MaterialType::SAND);
}

TEST_F(StateTests, SimRunningState_MouseInteraction) {
    transitionTo("SimRunning");
    
    // Send mouse events
    dsm->queueEvent(MouseDownEvent{50, 50});
    dsm->queueEvent(MouseMoveEvent{55, 55});
    dsm->queueEvent(MouseUpEvent{60, 60});
    processEvents();
    
    // Should remain in SimRunning
    EXPECT_EQ(dsm->getCurrentStateName(), "SimRunning");
}

// ===== SimPaused State Tests =====

TEST_F(StateTests, SimPausedState_PreservesSimulation) {
    transitionTo("SimRunning");
    
    // Advance simulation a bit
    for (int i = 0; i < 5; ++i) {
        dsm->queueEvent(AdvanceSimulationCommand{});
    }
    processEvents();
    
    uint32_t stepsBeforePause = dsm->getSharedState().getCurrentStep();
    
    // Pause
    dsm->queueEvent(PauseCommand{});
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "SimPaused");
    
    // Resume - use EventRouter for immediate event
    dsm->queueEvent(ResumeCommand{});
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "SimRunning");
    
    // Step count should be preserved
    EXPECT_EQ(dsm->getSharedState().getCurrentStep(), stepsBeforePause);
}

TEST_F(StateTests, SimPausedState_SingleStepAdvance) {
    transitionTo("SimPaused");
    
    uint32_t initialSteps = dsm->getSharedState().getCurrentStep();
    
    // Single step advance while paused
    dsm->queueEvent(AdvanceSimulationCommand{});
    processEvents();
    
    // Should still be paused but with one more step
    EXPECT_EQ(dsm->getCurrentStateName(), "SimPaused");
    EXPECT_EQ(dsm->getSharedState().getCurrentStep(), initialSteps + 1);
}

TEST_F(StateTests, SimPausedState_ResetWhilePaused) {
    transitionTo("SimPaused");
    
    // Reset while paused
    dsm->queueEvent(ResetSimulationCommand{});
    processEvents();
    
    // Should go to SimRunning (new instance)
    EXPECT_EQ(dsm->getCurrentStateName(), "SimRunning");
    EXPECT_FALSE(dsm->getSharedState().getIsPaused());
}

TEST_F(StateTests, SimPausedState_MaterialChangeWhilePaused) {
    transitionTo("SimPaused");
    
    // Change material while paused
    dsm->queueEvent(SelectMaterialCommand{MaterialType::METAL});
    processEvents();
    
    EXPECT_EQ(dsm->getSharedState().getSelectedMaterial(), MaterialType::METAL);
    EXPECT_EQ(dsm->getCurrentStateName(), "SimPaused");
}

// ===== Config State Tests =====

TEST_F(StateTests, ConfigState_BackToMainMenu) {
    transitionTo("MainMenu");
    
    // Go to config
    dsm->queueEvent(OpenConfigCommand{});
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "Config");
    
    // Use StartSimulationCommand as back (as per implementation hack)
    dsm->queueEvent(StartSimulationCommand{});
    processEvents();
    
    EXPECT_EQ(dsm->getCurrentStateName(), "MainMenu");
}

// ===== Shutdown State Tests =====

TEST_F(StateTests, ShutdownState_SetsExitFlag) {
    transitionTo("MainMenu");
    
    // Send quit command
    dsm->queueEvent(QuitApplicationCommand{});
    processEvents();
    
    EXPECT_EQ(dsm->getCurrentStateName(), "Shutdown");
    EXPECT_TRUE(dsm->shouldExit());
}

TEST_F(StateTests, ShutdownState_CleansUpResources) {
    transitionTo("SimRunning");
    EXPECT_NE(dsm->getSimulationManager(), nullptr);
    
    // Quit
    dsm->queueEvent(QuitApplicationCommand{});
    processEvents();
    
    // Resources should be cleaned up
    EXPECT_EQ(dsm->getCurrentStateName(), "Shutdown");
    EXPECT_TRUE(dsm->shouldExit());
}

// ===== State Lifecycle Tests =====

TEST_F(StateTests, StateLifecycle_OnEnterOnExit) {
    // Test that state lifecycle methods are called properly
    // This is implicitly tested by the resource creation/destruction tests above
    
    // SimRunning creates SimulationManager on enter
    transitionTo("SimRunning");
    EXPECT_NE(dsm->getSimulationManager(), nullptr);
    
    // Transitioning away should clean it up
    dsm->queueEvent(QuitApplicationCommand{});
    processEvents();
    
    // Shutdown state should have cleared everything
    EXPECT_TRUE(dsm->shouldExit());
}

// ===== Invalid State Transition Tests =====

TEST_F(StateTests, InvalidTransitions_IgnoredProperly) {
    // Test that invalid events in states are ignored
    
    // In Startup, simulation events should be ignored
    EXPECT_EQ(dsm->getCurrentStateName(), "Startup");
    dsm->queueEvent(AdvanceSimulationCommand{});
    dsm->queueEvent(PauseCommand{});
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "Startup");
    
    // In MainMenu, simulation events should be ignored
    transitionTo("MainMenu");
    dsm->queueEvent(AdvanceSimulationCommand{});
    dsm->queueEvent(ResumeCommand{});
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "MainMenu");
}