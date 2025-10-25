#pragma once

#include "../DirtSimStateMachine.h"
#include "../Event.h"
#include "../EventRouter.h"
#include "LVGLTestBase.h"
#include <gtest/gtest.h>
#include <memory>

/**
 * @brief Base class for UI event testing.
 *
 * Provides common infrastructure for testing UI widget event generation
 * and routing through the event system. Handles state machine setup,
 * event processing, and provides convenient helper methods.
 */
class UIEventTestBase : public LVGLTestBase {
protected:
    void SetUp() override
    {
        // Call base class setup (handles LVGL initialization).
        LVGLTestBase::SetUp();

        // Create state machine with event router.
        stateMachine_ = std::make_unique<DirtSim::DirtSimStateMachine>(lv_display_get_default());

        // Transition to SimRunning state for testing.
        stateMachine_->handleEvent(InitCompleteEvent{});
        stateMachine_->handleEvent(StartSimulationCommand{});

        // Get active screen for widget creation.
        screen_ = lv_screen_active();

        spdlog::info("[TEST] UIEventTestBase setup complete - ready for UI interaction tests");
    }

    void TearDown() override
    {
        stateMachine_.reset();
        LVGLTestBase::TearDown();
    }

    /**
     * @brief Process all queued events in the state machine.
     *
     * Call this after triggering UI events to let the state machine process them.
     */
    void processEvents()
    {
        stateMachine_->eventProcessor.processEventsFromQueue(*stateMachine_);
    }

    /**
     * @brief Get the world instance for state verification.
     * @return Pointer to world (never null in tests).
     */
    WorldInterface* getWorld()
    {
        auto* world = stateMachine_->getSimulationManager()->getWorld();
        if (!world) {
            throw std::runtime_error("World is null - test setup failed");
        }
        return world;
    }

    /**
     * @brief Get the event router for widget creation.
     * @return Pointer to event router.
     */
    EventRouter* getRouter() { return &stateMachine_->getEventRouter(); }

    /**
     * @brief Get the screen for widget creation.
     * @return LVGL screen object.
     */
    lv_obj_t* getScreen() { return screen_; }

    /**
     * @brief Get the state machine for advanced test scenarios.
     * @return Reference to state machine.
     */
    DirtSim::DirtSimStateMachine& getStateMachine() { return *stateMachine_; }

protected:
    lv_obj_t* screen_ = nullptr;
    std::unique_ptr<DirtSim::DirtSimStateMachine> stateMachine_;
};
