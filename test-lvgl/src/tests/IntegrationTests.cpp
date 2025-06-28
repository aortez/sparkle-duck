#include <gtest/gtest.h>
#include "../DirtSimStateMachine.h"
#include "../EventRouter.h"
#include "../Event.h"
#include "../LvglEventFactory.h"
#include <memory>
#include <thread>
#include <chrono>

using namespace DirtSim;

// Mock UI interaction test fixture.
class IntegrationTests : public ::testing::Test {
protected:
    std::unique_ptr<DirtSimStateMachine> dsm;
    
    void SetUp() override {
        dsm = std::make_unique<DirtSimStateMachine>();
        // Get to SimRunning state for most tests.
        dsm->queueEvent(InitCompleteEvent{});
        dsm->queueEvent(StartSimulationCommand{});
        processEvents();
    }
    
    void processEvents() {
        dsm->eventProcessor.processEventsFromQueue(*dsm);
    }
    
    // Simulate UI button clicks and interactions.
    void simulateButtonClick(const std::string& button) {
        auto& router = dsm->getEventRouter();
        
        if (button == "pause") {
            router.routeEvent(Event{PauseCommand{}});
        } else if (button == "resume") {
            router.routeEvent(Event{ResumeCommand{}});
        } else if (button == "reset") {
            router.routeEvent(Event{ResetSimulationCommand{}});
        } else if (button == "screenshot") {
            router.routeEvent(Event{CaptureScreenshotCommand{}});
        } else if (button == "quit") {
            router.routeEvent(Event{QuitApplicationCommand{}});
        }
    }
    
    void simulateSliderChange(const std::string& slider, double value) {
        auto& router = dsm->getEventRouter();
        
        if (slider == "timescale") {
            router.routeEvent(Event{SetTimescaleCommand{value}});
        } else if (slider == "elasticity") {
            router.routeEvent(Event{SetElasticityCommand{value}});
        }
    }
    
    void simulateMaterialSelection(MaterialType material) {
        auto& router = dsm->getEventRouter();
        router.routeEvent(Event{SelectMaterialCommand{material}});
    }
    
    void simulateMouseDrag(int startX, int startY, int endX, int endY) {
        auto& router = dsm->getEventRouter();
        router.routeEvent(Event{MouseDownEvent{startX, startY}});
        
        // Simulate drag with intermediate points.
        int steps = 5;
        for (int i = 1; i <= steps; ++i) {
            int x = startX + (endX - startX) * i / steps;
            int y = startY + (endY - startY) * i / steps;
            router.routeEvent(Event{MouseMoveEvent{x, y}});
        }
        
        router.routeEvent(Event{MouseUpEvent{endX, endY}});
    }
};

// ===== UI Workflow Tests =====

TEST_F(IntegrationTests, UIWorkflow_PauseResumeReset) {
    EXPECT_EQ(dsm->getCurrentStateName(), "SimRunning");
    auto& sharedState = dsm->getSharedState();
    
    // Simulate running for a bit.
    for (int i = 0; i < 10; ++i) {
        dsm->queueEvent(AdvanceSimulationCommand{});
    }
    processEvents();
    uint32_t stepsBeforePause = sharedState.getCurrentStep();
    
    // User clicks pause.
    simulateButtonClick("pause");
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "SimPaused");
    EXPECT_TRUE(sharedState.getIsPaused());
    
    // User clicks resume.
    simulateButtonClick("resume");
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "SimRunning");
    EXPECT_FALSE(sharedState.getIsPaused());
    EXPECT_EQ(sharedState.getCurrentStep(), stepsBeforePause);
    
    // User clicks reset.
    simulateButtonClick("reset");
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "SimRunning");
}

TEST_F(IntegrationTests, UIWorkflow_MaterialSelectionAndDrawing) {
    auto& sharedState = dsm->getSharedState();
    
    // Select water material.
    simulateMaterialSelection(MaterialType::WATER);
    processEvents();
    EXPECT_EQ(sharedState.getSelectedMaterial(), MaterialType::WATER);
    
    // Draw with water.
    simulateMouseDrag(100, 100, 150, 150);
    processEvents();
    
    // Change to sand.
    simulateMaterialSelection(MaterialType::SAND);
    processEvents();
    EXPECT_EQ(sharedState.getSelectedMaterial(), MaterialType::SAND);
    
    // Draw with sand.
    simulateMouseDrag(200, 200, 250, 250);
    processEvents();
}

TEST_F(IntegrationTests, UIWorkflow_PhysicsParameterAdjustment) {
    auto& sharedState = dsm->getSharedState();
    
    // Adjust timescale.
    simulateSliderChange("timescale", 0.5);
    processEvents();
    
    auto params = sharedState.getPhysicsParams();
    EXPECT_FLOAT_EQ(params.timescale, 0.5);
    
    // Adjust elasticity.
    simulateSliderChange("elasticity", 0.8);
    processEvents();
    
    params = sharedState.getPhysicsParams();
    EXPECT_FLOAT_EQ(params.elasticity, 0.8);
}

TEST_F(IntegrationTests, UIWorkflow_DrawingWhilePaused) {
    auto& sharedState = dsm->getSharedState();
    
    // Pause simulation.
    simulateButtonClick("pause");
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "SimPaused");
    
    // Select material and draw while paused.
    simulateMaterialSelection(MaterialType::DIRT);
    processEvents();
    
    simulateMouseDrag(50, 50, 100, 100);
    processEvents();
    
    // Should still be paused.
    EXPECT_EQ(dsm->getCurrentStateName(), "SimPaused");
    EXPECT_EQ(sharedState.getSelectedMaterial(), MaterialType::DIRT);
}

// ===== Complex UI Scenarios =====

TEST_F(IntegrationTests, ComplexScenario_MultipleUsersInteracting) {
    // Simulate multiple UI interactions happening quickly.
    auto& sharedState = dsm->getSharedState();
    
    // User rapidly clicking pause/resume.
    for (int i = 0; i < 5; ++i) {
        simulateButtonClick("pause");
        processEvents();
        EXPECT_TRUE(sharedState.getIsPaused());
        simulateButtonClick("resume");
        processEvents();
        EXPECT_FALSE(sharedState.getIsPaused());
    }
    
    // While also adjusting sliders.
    simulateSliderChange("timescale", 0.1);
    simulateSliderChange("elasticity", 0.9);
    processEvents();
    
    // And drawing.
    simulateMouseDrag(0, 0, 100, 100);
    processEvents();
    
    // System should remain stable.
    EXPECT_EQ(dsm->getCurrentStateName(), "SimRunning");
}

TEST_F(IntegrationTests, ComplexScenario_SimulationUnderLoad) {
    auto& sharedState = dsm->getSharedState();
    
    // Start heavy simulation activity.
    std::thread simThread([this]() {
        for (int i = 0; i < 100; ++i) {
            dsm->queueEvent(AdvanceSimulationCommand{});
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    // While user interacts with UI.
    std::thread uiThread([this]() {
        for (int i = 0; i < 10; ++i) {
            simulateButtonClick("pause");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            simulateButtonClick("resume");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    
    simThread.join();
    uiThread.join();
    processEvents();
    
    // System should handle concurrent load.
    EXPECT_GT(sharedState.getCurrentStep(), 0);
}

// ===== Event Factory Integration =====

TEST_F(IntegrationTests, EventFactory_MouseEventConversion) {
    // Test that mouse events from UI are properly converted.
    auto& router = dsm->getEventRouter();
    
    // Simulate LVGL mouse event data.
    struct MockLvglEventData {
        int x, y;
        bool pressed;
    } lvglData = {100, 200, true};
    
    // Convert to our event system.
    if (lvglData.pressed) {
        router.routeEvent(Event{MouseDownEvent{lvglData.x, lvglData.y}});
    } else {
        router.routeEvent(Event{MouseUpEvent{lvglData.x, lvglData.y}});
    }
    
    processEvents();
    
    // Event should be processed.
    EXPECT_EQ(dsm->getCurrentStateName(), "SimRunning");
}

TEST_F(IntegrationTests, EventFactory_ButtonMatrixConversion) {
    auto& router = dsm->getEventRouter();
    
    // Simulate button matrix selection (like world type selection).
    // User selects WorldB (RulesB).
    router.routeEvent(Event{SwitchWorldTypeCommand{WorldType::RulesB}});
    processEvents();
    
    // Note: SharedSimState doesn't track world type, so we can't verify this.
    // The actual world type change happens in the state machine.
}

// ===== Error Handling Tests =====

TEST_F(IntegrationTests, ErrorHandling_InvalidStateForUI) {
    // Go to shutdown state.
    dsm->queueEvent(QuitApplicationCommand{});
    processEvents();
    EXPECT_EQ(dsm->getCurrentStateName(), "Shutdown");
    
    // Try UI interactions in shutdown state.
    simulateButtonClick("pause");
    simulateButtonClick("reset");
    simulateMaterialSelection(MaterialType::WATER);
    processEvents();
    
    // Should remain in shutdown.
    EXPECT_EQ(dsm->getCurrentStateName(), "Shutdown");
    EXPECT_TRUE(dsm->shouldExit());
}

TEST_F(IntegrationTests, ErrorHandling_RapidStateChanges) {
    // Rapidly change states.
    simulateButtonClick("pause");
    simulateButtonClick("reset");
    simulateButtonClick("pause");
    simulateButtonClick("resume");
    simulateButtonClick("reset");
    
    processEvents();
    
    // System should stabilize in a valid state.
    std::string state = dsm->getCurrentStateName();
    EXPECT_TRUE(state == "SimRunning" || state == "SimPaused");
}

// ===== Performance Under UI Load =====

TEST_F(IntegrationTests, Performance_UIEventStorm) {
    const int numEvents = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    
    // Simulate rapid UI interactions.
    for (int i = 0; i < numEvents; ++i) {
        if (i % 4 == 0) simulateButtonClick("pause");
        else if (i % 4 == 1) simulateButtonClick("resume");
        else if (i % 4 == 2) simulateMaterialSelection(static_cast<MaterialType>(i % 8));
        else simulateSliderChange("timescale", (i % 10) / 10.0);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should handle high UI event rate.
    EXPECT_LT(duration.count(), 1000) << "UI event processing took too long: " 
                                      << duration.count() << "ms for " << numEvents << " events";
    
    // Process any queued events.
    processEvents();
    
    // System should still be functional.
    EXPECT_FALSE(dsm->shouldExit());
}