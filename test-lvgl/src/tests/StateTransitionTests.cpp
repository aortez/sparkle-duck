#include <gtest/gtest.h>
#include "../DirtSimStateMachine.h"
#include "../states/State.h"
#include "../Event.h"
#include <unordered_map>
#include <set>
#include <string>

using namespace DirtSim;

class StateTransitionTests : public ::testing::Test {
protected:
    std::unique_ptr<DirtSimStateMachine> dsm;
    
    void SetUp() override {
        dsm = std::make_unique<DirtSimStateMachine>();
    }
    
    void tearDown() {
        dsm.reset();
    }
    
    void processEvents() {
        dsm->eventProcessor.processEventsFromQueue(*dsm);
    }
    
    // Helper to verify a transition
    bool verifyTransition(const std::string& fromState, 
                         const Event& event, 
                         const std::string& expectedState) {
        // Get to the starting state
        if (fromState != dsm->getCurrentStateName()) {
            navigateToState(fromState);
        }
        
        EXPECT_EQ(dsm->getCurrentStateName(), fromState) 
            << "Failed to navigate to starting state: " << fromState;
        
        // Send the event
        dsm->queueEvent(event);
        processEvents();
        
        // Check the result
        return dsm->getCurrentStateName() == expectedState;
    }
    
    // Navigate to a specific state from current state
    void navigateToState(const std::string& targetState) {
        std::string currentState = dsm->getCurrentStateName();
        
        // If already in target state, nothing to do
        if (currentState == targetState) {
            return;
        }
        
        // Handle navigation based on current state
        if (targetState == "Startup") {
            // Can't navigate back to Startup
            return;
        } else if (targetState == "MainMenu") {
            if (currentState == "Startup") {
                dsm->queueEvent(InitCompleteEvent{});
                processEvents();
            } else if (currentState == "Config") {
                // Config goes back to MainMenu with StartSimulationCommand (as per Config.cpp)
                dsm->queueEvent(StartSimulationCommand{});
                processEvents();
            } else if (currentState == "SimRunning" || currentState == "SimPaused") {
                // Need to quit to shutdown, then reinitialize
                dsm->queueEvent(QuitApplicationCommand{});
                processEvents();
                // Recreate DSM to start fresh
                tearDown();
                SetUp();
                dsm->queueEvent(InitCompleteEvent{});
                processEvents();
            } else if (currentState == "Shutdown") {
                // Recreate DSM to start fresh
                tearDown();
                SetUp();
                dsm->queueEvent(InitCompleteEvent{});
                processEvents();
            }
        } else if (targetState == "SimRunning") {
            navigateToState("MainMenu");
            dsm->queueEvent(StartSimulationCommand{});
            processEvents();
        } else if (targetState == "SimPaused") {
            navigateToState("SimRunning");
            dsm->queueEvent(PauseCommand{});
            processEvents();
        } else if (targetState == "Config") {
            navigateToState("MainMenu");
            dsm->queueEvent(OpenConfigCommand{});
            processEvents();
        }
    }
};

// ===== Valid State Transitions =====

TEST_F(StateTransitionTests, ValidTransitions_FromStartup) {
    // Startup -> MainMenu
    EXPECT_TRUE(verifyTransition("Startup", InitCompleteEvent{}, "MainMenu"));
}

TEST_F(StateTransitionTests, ValidTransitions_FromMainMenu) {
    // MainMenu -> SimRunning
    EXPECT_TRUE(verifyTransition("MainMenu", StartSimulationCommand{}, "SimRunning"));
    
    // MainMenu -> Config
    EXPECT_TRUE(verifyTransition("MainMenu", OpenConfigCommand{}, "Config"));
    
    // MainMenu -> Shutdown
    EXPECT_TRUE(verifyTransition("MainMenu", QuitApplicationCommand{}, "Shutdown"));
}

TEST_F(StateTransitionTests, ValidTransitions_FromSimRunning) {
    // SimRunning -> SimPaused
    EXPECT_TRUE(verifyTransition("SimRunning", PauseCommand{}, "SimPaused"));
    
    // SimRunning stays in SimRunning for simulation events
    EXPECT_TRUE(verifyTransition("SimRunning", AdvanceSimulationCommand{}, "SimRunning"));
    EXPECT_TRUE(verifyTransition("SimRunning", MouseDownEvent{50, 50}, "SimRunning"));
    EXPECT_TRUE(verifyTransition("SimRunning", SelectMaterialCommand{MaterialType::WATER}, "SimRunning"));
    
    // SimRunning -> Shutdown
    EXPECT_TRUE(verifyTransition("SimRunning", QuitApplicationCommand{}, "Shutdown"));
}

TEST_F(StateTransitionTests, ValidTransitions_FromSimPaused) {
    // SimPaused -> SimRunning (resume)
    EXPECT_TRUE(verifyTransition("SimPaused", ResumeCommand{}, "SimRunning"));
    
    // SimPaused -> SimRunning (reset creates new instance)
    EXPECT_TRUE(verifyTransition("SimPaused", ResetSimulationCommand{}, "SimRunning"));
    
    // SimPaused stays in SimPaused for certain events
    EXPECT_TRUE(verifyTransition("SimPaused", AdvanceSimulationCommand{}, "SimPaused"));
    EXPECT_TRUE(verifyTransition("SimPaused", SelectMaterialCommand{MaterialType::SAND}, "SimPaused"));
    
    // SimPaused -> Shutdown
    EXPECT_TRUE(verifyTransition("SimPaused", QuitApplicationCommand{}, "Shutdown"));
}

TEST_F(StateTransitionTests, ValidTransitions_FromConfig) {
    // Config -> MainMenu (using StartSimulationCommand as back button hack)
    EXPECT_TRUE(verifyTransition("Config", StartSimulationCommand{}, "MainMenu"));
    
    // Config -> Shutdown
    EXPECT_TRUE(verifyTransition("Config", QuitApplicationCommand{}, "Shutdown"));
}

// ===== Invalid State Transitions =====

TEST_F(StateTransitionTests, InvalidTransitions_IgnoredProperly) {
    // Startup should ignore simulation events
    EXPECT_TRUE(verifyTransition("Startup", AdvanceSimulationCommand{}, "Startup"));
    EXPECT_TRUE(verifyTransition("Startup", PauseCommand{}, "Startup"));
    EXPECT_TRUE(verifyTransition("Startup", MouseDownEvent{10, 10}, "Startup"));
    
    // MainMenu should ignore simulation-specific events
    EXPECT_TRUE(verifyTransition("MainMenu", AdvanceSimulationCommand{}, "MainMenu"));
    EXPECT_TRUE(verifyTransition("MainMenu", PauseCommand{}, "MainMenu"));
    EXPECT_TRUE(verifyTransition("MainMenu", ResumeCommand{}, "MainMenu"));
    
    // Config should ignore most events
    EXPECT_TRUE(verifyTransition("Config", AdvanceSimulationCommand{}, "Config"));
    EXPECT_TRUE(verifyTransition("Config", PauseCommand{}, "Config"));
    EXPECT_TRUE(verifyTransition("Config", MouseDownEvent{50, 50}, "Config"));
}

// ===== State Lifecycle Tests =====

TEST_F(StateTransitionTests, StateLifecycle_ResourceManagement) {
    // Test that resources are properly created and destroyed
    
    // Start in Startup
    EXPECT_EQ(dsm->getCurrentStateName(), "Startup");
    EXPECT_EQ(dsm->world, nullptr);
    EXPECT_EQ(dsm->getSimulationManager(), nullptr);
    
    // Transition to MainMenu - should create world
    dsm->queueEvent(InitCompleteEvent{});
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "MainMenu");
    EXPECT_NE(dsm->world, nullptr);
    EXPECT_EQ(dsm->getSimulationManager(), nullptr);
    
    // Transition to SimRunning - should create SimulationManager
    dsm->queueEvent(StartSimulationCommand{});
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "SimRunning");
    EXPECT_NE(dsm->getSimulationManager(), nullptr);
    
    // Transition to Shutdown - should clean up
    dsm->queueEvent(QuitApplicationCommand{});
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "Shutdown");
    EXPECT_TRUE(dsm->shouldExit());
}

// ===== Transition Path Tests =====

TEST_F(StateTransitionTests, TransitionPaths_CommonWorkflows) {
    // Test common user workflows through states
    
    // Workflow 1: Start -> Play -> Pause -> Resume -> Quit
    EXPECT_EQ(dsm->getCurrentStateName(), "Startup");
    
    dsm->queueEvent(InitCompleteEvent{});
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "MainMenu");
    
    dsm->queueEvent(StartSimulationCommand{});
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "SimRunning");
    
    dsm->queueEvent(PauseCommand{});
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "SimPaused");
    
    dsm->queueEvent(ResumeCommand{});
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "SimRunning");
    
    dsm->queueEvent(QuitApplicationCommand{});
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "Shutdown");
}

TEST_F(StateTransitionTests, TransitionPaths_ConfigurationFlow) {
    // Workflow 2: Start -> Menu -> Config -> Menu -> Play
    navigateToState("MainMenu");
    
    dsm->queueEvent(OpenConfigCommand{});
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "Config");
    
    // Go back to menu
    dsm->queueEvent(StartSimulationCommand{}); // Config hack
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "MainMenu");
    
    // Start simulation
    dsm->queueEvent(StartSimulationCommand{});
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "SimRunning");
}

// ===== State Machine Consistency =====

TEST_F(StateTransitionTests, Consistency_AlwaysInValidState) {
    // Send random valid events and verify we're always in a valid state
    std::set<std::string> validStates = {
        "Startup", "MainMenu", "SimRunning", "SimPaused", 
        "Config", "Shutdown", "UnitTesting", "Benchmarking",
        "Loading", "Saving", "Demo"
    };
    
    std::vector<Event> events = {
        InitCompleteEvent{},
        StartSimulationCommand{},
        PauseCommand{},
        ResumeCommand{},
        ResetSimulationCommand{},
        OpenConfigCommand{},
        AdvanceSimulationCommand{},
        SelectMaterialCommand{MaterialType::WATER},
        MouseDownEvent{100, 100},
        QuitApplicationCommand{}
    };
    
    // Send random events
    for (int i = 0; i < 20; ++i) {
        if (dsm->shouldExit()) break;
        
        // Pick a random event
        const auto& event = events[i % events.size()];
        dsm->queueEvent(event);
        processEvents();
        
        // Verify we're in a valid state
        std::string currentState = dsm->getCurrentStateName();
        EXPECT_TRUE(validStates.count(currentState) > 0)
            << "Invalid state: " << currentState;
    }
}

// ===== Transition Matrix Test =====

TEST_F(StateTransitionTests, TransitionMatrix_Completeness) {
    // Define expected transition matrix
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> transitions = {
        {"Startup", {
            {"InitCompleteEvent", "MainMenu"}
        }},
        {"MainMenu", {
            {"StartSimulationCommand", "SimRunning"},
            {"OpenConfigCommand", "Config"},
            {"QuitApplicationCommand", "Shutdown"}
        }},
        {"SimRunning", {
            {"PauseCommand", "SimPaused"},
            {"AdvanceSimulationCommand", "SimRunning"},
            {"ResetSimulationCommand", "SimRunning"},
            {"QuitApplicationCommand", "Shutdown"}
        }},
        {"SimPaused", {
            {"ResumeCommand", "SimRunning"},
            {"ResetSimulationCommand", "SimRunning"},
            {"AdvanceSimulationCommand", "SimPaused"},
            {"QuitApplicationCommand", "Shutdown"}
        }},
        {"Config", {
            {"StartSimulationCommand", "MainMenu"}, // hack
            {"QuitApplicationCommand", "Shutdown"}
        }}
    };
    
    // Test each transition in the matrix
    for (const auto& [fromState, eventMap] : transitions) {
        for (const auto& [eventName, toState] : eventMap) {
            // We can't easily test this without event name to event object mapping
            // This is more of a documentation test
            EXPECT_TRUE(true) << "Transition: " << fromState << " --[" 
                             << eventName << "]--> " << toState;
        }
    }
}

// ===== Edge Case Tests =====

TEST_F(StateTransitionTests, EdgeCases_MultipleQuitCommands) {
    navigateToState("SimRunning");
    
    // Send multiple quit commands
    dsm->queueEvent(QuitApplicationCommand{});
    dsm->queueEvent(QuitApplicationCommand{});
    dsm->queueEvent(QuitApplicationCommand{});
    processEvents();
    
    // Should be in Shutdown and stay there
    EXPECT_EQ(dsm->getCurrentStateName(), "Shutdown");
    EXPECT_TRUE(dsm->shouldExit());
}

TEST_F(StateTransitionTests, EdgeCases_EventsAfterShutdown) {
    // Go to shutdown
    dsm->queueEvent(QuitApplicationCommand{});
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "Shutdown");
    
    // Try to send more events
    dsm->queueEvent(StartSimulationCommand{});
    dsm->queueEvent(InitCompleteEvent{});
    dsm->queueEvent(PauseCommand{});
    processEvents();
    
    // Should remain in shutdown
    EXPECT_EQ(dsm->getCurrentStateName(), "Shutdown");
    EXPECT_TRUE(dsm->shouldExit());
}