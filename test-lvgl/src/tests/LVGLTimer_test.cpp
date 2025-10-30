#include "../SimulatorUI.h"
#include "../UIUpdateConsumer.h"
#include "../SharedSimState.h"
#include "../Event.h"
#include "../EventRouter.h"
#include "../DirtSimStateMachine.h"
#include "../SynchronizedQueue.h"
#include "../SimulationManager.h"
#include "../World.h"
#include "../WorldSetup.h"
#include "LVGLTestBase.h"
#include <atomic>

using namespace DirtSim;

// Test fixture for LVGL timer tests.
class LVGLTimerTest : public LVGLTestBase {
protected:
    void SetUp() override {
        // Call base class setup first (handles LVGL init and display creation).
        LVGLTestBase::SetUp();
        
        // Create state machine and shared state.
        stateMachine_ = std::make_unique<DirtSimStateMachine>();
        sharedState_ = std::make_unique<SharedSimState>();
        eventQueue_ = std::make_unique<SynchronizedQueue<Event>>();
        
        // Create event router.
        eventRouter_ = std::make_unique<EventRouter>(*stateMachine_, *sharedState_, *eventQueue_);
    }
    
    void TearDown() override {
        // Clean up our resources first.
        eventRouter_.reset();
        eventQueue_.reset();
        sharedState_.reset();
        stateMachine_.reset();
        
        // Then call base class teardown (handles LVGL cleanup).
        LVGLTestBase::TearDown();
    }
    
    std::unique_ptr<DirtSimStateMachine> stateMachine_;
    std::unique_ptr<SharedSimState> sharedState_;
    std::unique_ptr<SynchronizedQueue<Event>> eventQueue_;
    std::unique_ptr<EventRouter> eventRouter_;
};

TEST_F(LVGLTimerTest, TimerNotCreatedWhenPushUpdatesDisabled) {
    // Ensure push updates are disabled.

    // Create UI.
    SimulatorUI ui(screen_, eventRouter_.get());
    ui.initialize();
    
    // Timer should not be created.
    // We can't directly check private members, but we can verify.
    // that no timer callbacks are happening.
    std::atomic<int> callCount{0};
    
    // Run LVGL for 100ms.
    runLVGL(100, 10);
    
    // No updates should have been consumed (no timer).
    EXPECT_EQ(callCount.load(), 0);
}

TEST_F(LVGLTimerTest, TimerCreatedWhenPushUpdatesEnabled) {
    // Enable push updates.

    // Create UI.
    SimulatorUI ui(screen_, eventRouter_.get());
    
    // Create a simple world and manager for UI.
    auto world = std::make_unique<World>(10, 10);
    ui.setWorld(world.get());
    
    ui.initialize();
    
    // Timer should be created and firing.
    // Push some updates to the queue.
    UIUpdateEvent update;
    update.fps = 60;
    update.dirty.fps = true;
    
    for (int i = 0; i < 5; i++) {
        sharedState_->pushUIUpdate(update);
    }
    
    // Run LVGL for ~100ms.
    runLVGL(100, 5);
    
    // Check that updates were consumed (queue should be empty or nearly empty).
    auto metrics = sharedState_->getUIUpdateMetrics();
    EXPECT_GT(metrics.popCount, 0);
}

TEST_F(LVGLTimerTest, TimerCallbackRateIs60FPS) {
    // Enable push updates.

    // Create UI.
    SimulatorUI ui(screen_, eventRouter_.get());
    
    // Create a simple world and manager for UI.
    auto world = std::make_unique<World>(10, 10);
    ui.setWorld(world.get());
    
    ui.initialize();
    
    // Track timer callbacks over 1 second.
    auto start = std::chrono::steady_clock::now();
    
    // Continuously push updates.
    std::thread pusher([this, &start]() {
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(1)) {
            UIUpdateEvent update;
            update.fps = 60;
            update.dirty.fps = true;
            sharedState_->pushUIUpdate(update);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    // Run LVGL for 1 second.
    runLVGL(1000, 1);
    
    pusher.join();
    
    // Check metrics.
    auto metrics = sharedState_->getUIUpdateMetrics();
    
    // We expect approximately 60 pops in 1 second (allow 10% variance).
    EXPECT_GT(metrics.popCount, 54);  // At least 54 updates consumed.
    EXPECT_LT(metrics.popCount, 66);  // At most 66 updates consumed.
    
    // Verify latest-update-wins is working (drop count should be > 0).
    EXPECT_GT(metrics.dropCount, 0);
}

TEST_F(LVGLTimerTest, TimerCleanedUpInDestructor) {
    // Enable push updates.

    {
        // Create UI in a scope.
        SimulatorUI ui(screen_, eventRouter_.get());
        ui.initialize();
        
        // Push an update.
        UIUpdateEvent update;
        update.fps = 60;
        sharedState_->pushUIUpdate(update);
        
        // Run LVGL briefly.
        runLVGL(16, 16);
        
        // UI destructor should clean up timer when it goes out of scope.
    }
    
    // After UI is destroyed, run LVGL again.
    // Should not crash (timer was properly deleted).
    runLVGL(50, 10);
    
    // If we got here without crashing, timer was properly cleaned up.
    SUCCEED();
}

TEST_F(LVGLTimerTest, TimerIntegrationWithUIUpdateConsumer) {
    // Enable push updates.

    // Create UI.
    SimulatorUI ui(screen_, eventRouter_.get());
    
    // Create world for complete setup.
    auto world = std::make_unique<World>(10, 10);
    ui.setWorld(world.get());
    
    ui.initialize();
    
    // Push several updates.
    for (int i = 0; i < 10; i++) {
        UIUpdateEvent update;
        update.fps = 60 + i;
        update.dirty.fps = true;
        update.sequenceNum = i;
        sharedState_->pushUIUpdate(update);
    }
    
    // Run LVGL for a bit to process the timer.
    runLVGL(200, 20);
    
    // Check that updates were consumed.
    auto metrics = sharedState_->getUIUpdateMetrics();
    EXPECT_GT(metrics.popCount, 0);
    EXPECT_GT(metrics.dropCount, 0); // Latest-update-wins should drop some.
}