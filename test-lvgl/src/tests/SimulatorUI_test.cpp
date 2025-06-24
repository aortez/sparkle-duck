#include "../SimulatorUI.h"
#include "../EventRouter.h"
#include "../DirtSimStateMachine.h"
#include "../SharedSimState.h"
#include "../SynchronizedQueue.h"
#include "LVGLTestBase.h"
#include <gtest/gtest.h>

using namespace DirtSim;

// Test fixture for SimulatorUI tests
class SimulatorUITest : public LVGLTestBase {
protected:
    void SetUp() override {
        // Call base class setup first (handles LVGL init and display creation)
        LVGLTestBase::SetUp();
        
        // Create dependencies
        stateMachine_ = std::make_unique<DirtSimStateMachine>();
        sharedState_ = std::make_unique<SharedSimState>();
        eventQueue_ = std::make_unique<SynchronizedQueue<Event>>();
        eventRouter_ = std::make_unique<EventRouter>(*stateMachine_, *sharedState_, *eventQueue_);
    }
    
    void TearDown() override {
        // Clean up our resources first
        eventRouter_.reset();
        eventQueue_.reset();
        sharedState_.reset();
        stateMachine_.reset();
        
        // Then call base class teardown
        LVGLTestBase::TearDown();
    }
    
    std::unique_ptr<DirtSimStateMachine> stateMachine_;
    std::unique_ptr<SharedSimState> sharedState_;
    std::unique_ptr<SynchronizedQueue<Event>> eventQueue_;
    std::unique_ptr<EventRouter> eventRouter_;
};

// Test that SimulatorUI can be created and initialized with proper LVGL setup
TEST_F(SimulatorUITest, InitializeWithValidDisplay) {
    // Create UI with valid display (from base class)
    SimulatorUI ui(screen_, eventRouter_.get());
    
    // Should initialize without throwing
    EXPECT_NO_THROW(ui.initialize());
}

// Test error handling when LVGL is not initialized
TEST(SimulatorUIErrorTest, InitializeWithoutLVGL) {
    // Don't call lv_init() - simulate uninitialized state
    
    // Create minimal dependencies
    DirtSimStateMachine stateMachine;
    SharedSimState sharedState;
    SynchronizedQueue<Event> eventQueue;
    EventRouter eventRouter(stateMachine, sharedState, eventQueue);
    
    // Try to create UI without LVGL initialized
    // Note: We can't actually create a screen without lv_init(), so we pass nullptr
    SimulatorUI ui(nullptr, &eventRouter);
    
    // Should throw with clear error message
    EXPECT_THROW({
        try {
            ui.initialize();
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(), "LVGL must be initialized before creating SimulatorUI");
            throw;
        }
    }, std::runtime_error);
}

// Test error handling when no display exists
TEST(SimulatorUIErrorTest, InitializeWithoutDisplay) {
    // Initialize LVGL but don't create display
    lv_init();
    
    // Create minimal dependencies
    DirtSimStateMachine stateMachine;
    SharedSimState sharedState;
    SynchronizedQueue<Event> eventQueue;
    EventRouter eventRouter(stateMachine, sharedState, eventQueue);
    
    // Create UI with a dummy non-null pointer (we'll catch the error before it's used)
    lv_obj_t* fake_screen = reinterpret_cast<lv_obj_t*>(0x1234);
    SimulatorUI ui(fake_screen, &eventRouter);
    
    // Should throw with clear error message about display
    EXPECT_THROW({
        try {
            ui.initialize();
        } catch (const std::runtime_error& e) {
            // Check that error message mentions display
            std::string error_msg(e.what());
            EXPECT_TRUE(error_msg.find("display") != std::string::npos);
            throw;
        }
    }, std::runtime_error);
    
    // Clean up
    lv_deinit();
}

// Test error handling with null screen
TEST(SimulatorUIErrorTest, InitializeWithNullScreen) {
    // Properly set up LVGL with display
    lv_init();
    lv_display_t* display = LVGLTestBase::createTestDisplay();
    
    // Create minimal dependencies
    DirtSimStateMachine stateMachine;
    SharedSimState sharedState;
    SynchronizedQueue<Event> eventQueue;
    EventRouter eventRouter(stateMachine, sharedState, eventQueue);
    
    // Create UI with null screen
    SimulatorUI ui(nullptr, &eventRouter);
    
    // Should throw with clear error message
    EXPECT_THROW({
        try {
            ui.initialize();
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(), "SimulatorUI requires a valid screen object");
            throw;
        }
    }, std::runtime_error);
    
    // Clean up
    lv_display_delete(display);
    lv_deinit();
}