#include "../UIUpdateConsumer.h"
#include "../SharedSimState.h"
#include "../SimulatorUI.h"
#include "../Event.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// Mock SimulatorUI for testing
class MockSimulatorUI : public SimulatorUI {
public:
    MockSimulatorUI() : SimulatorUI(nullptr, nullptr) {}
    
    // Track applied updates for testing
    std::vector<UIUpdateEvent> applied_updates;
    
    void applyUpdate(const UIUpdateEvent& update) {
        applied_updates.push_back(update);
    }
};

class UIUpdateConsumerTest : public ::testing::Test {
protected:
    void SetUp() override {
        sim_state_ = std::make_unique<SharedSimState>();
        ui_ = std::make_unique<MockSimulatorUI>();
        consumer_ = std::make_unique<UIUpdateConsumer>(sim_state_.get(), ui_.get());
    }

    // Helper to create a test update
    UIUpdateEvent createTestUpdate(uint64_t seq_num = 1) {
        UIUpdateEvent update;
        update.sequenceNum = seq_num;
        update.timestamp = std::chrono::steady_clock::now();
        update.fps = 60;
        update.isPaused = false;
        update.stepCount = 1000;
        return update;
    }

    std::unique_ptr<SharedSimState> sim_state_;
    std::unique_ptr<MockSimulatorUI> ui_;
    std::unique_ptr<UIUpdateConsumer> consumer_;
};

TEST_F(UIUpdateConsumerTest, ConstructionRequiresNonNullPointers) {
    // Test null SharedSimState
    EXPECT_THROW(UIUpdateConsumer(nullptr, ui_.get()), std::runtime_error);
    
    // Test null SimulatorUI
    EXPECT_THROW(UIUpdateConsumer(sim_state_.get(), nullptr), std::runtime_error);
}

TEST_F(UIUpdateConsumerTest, ConsumeUpdateReturnsFalseWhenDisabled) {
    // Push updates are disabled by default
    EXPECT_FALSE(consumer_->isPushUpdatesEnabled());
    
    // Push an update
    sim_state_->pushUIUpdate(createTestUpdate());
    
    // Consume should return false when disabled
    EXPECT_FALSE(consumer_->consumeUpdate());
    
    // Metrics should show no updates consumed
    auto metrics = consumer_->getMetrics();
    EXPECT_EQ(0u, metrics.updates_consumed);
}

TEST_F(UIUpdateConsumerTest, ConsumeUpdateReturnsFalseWhenQueueEmpty) {
    // Enable push updates
    sim_state_->enablePushUpdates(true);
    EXPECT_TRUE(consumer_->isPushUpdatesEnabled());
    
    // Consume with empty queue should return false
    EXPECT_FALSE(consumer_->consumeUpdate());
    
    // Metrics should show no updates consumed
    auto metrics = consumer_->getMetrics();
    EXPECT_EQ(0u, metrics.updates_consumed);
}

TEST_F(UIUpdateConsumerTest, ConsumeUpdateSuccessfullyConsumesWhenEnabled) {
    // Enable push updates
    sim_state_->enablePushUpdates(true);
    
    // Push an update
    auto update = createTestUpdate(1);
    sim_state_->pushUIUpdate(update);
    
    // Consume should return true
    EXPECT_TRUE(consumer_->consumeUpdate());
    
    // Metrics should show one update consumed
    auto metrics = consumer_->getMetrics();
    EXPECT_EQ(1u, metrics.updates_consumed);
    EXPECT_EQ(0u, metrics.updates_missed);
}

TEST_F(UIUpdateConsumerTest, DetectsMissedUpdates) {
    // Enable push updates
    sim_state_->enablePushUpdates(true);
    
    // Consume first update
    sim_state_->pushUIUpdate(createTestUpdate(1));
    EXPECT_TRUE(consumer_->consumeUpdate());
    
    // Push update with sequence gap (simulating dropped updates)
    sim_state_->pushUIUpdate(createTestUpdate(5));
    EXPECT_TRUE(consumer_->consumeUpdate());
    
    // Should detect 3 missed updates (2, 3, 4)
    auto metrics = consumer_->getMetrics();
    EXPECT_EQ(2u, metrics.updates_consumed);
    EXPECT_EQ(3u, metrics.updates_missed);
}

TEST_F(UIUpdateConsumerTest, TracksLatencyMetrics) {
    // Enable push updates
    sim_state_->enablePushUpdates(true);
    
    // Create update with known timestamp
    auto update = createTestUpdate(1);
    auto past_time = std::chrono::steady_clock::now() - std::chrono::milliseconds(10);
    update.timestamp = past_time;
    
    // Push and consume
    sim_state_->pushUIUpdate(update);
    std::this_thread::sleep_for(std::chrono::milliseconds(5)); // Add some latency
    EXPECT_TRUE(consumer_->consumeUpdate());
    
    // Check latency metrics
    auto metrics = consumer_->getMetrics();
    EXPECT_GT(metrics.avg_latency_ms, 10.0); // Should be > 10ms (original delay)
    EXPECT_LT(metrics.avg_latency_ms, 20.0); // But < 20ms
    EXPECT_EQ(metrics.avg_latency_ms, metrics.max_latency_ms); // First update
    EXPECT_EQ(metrics.avg_latency_ms, metrics.min_latency_ms); // First update
}

TEST_F(UIUpdateConsumerTest, UpdatesLatencyMetricsOverTime) {
    // Enable push updates
    sim_state_->enablePushUpdates(true);
    
    // Consume multiple updates with different latencies
    for (int i = 1; i <= 5; i++) {
        auto update = createTestUpdate(i);
        auto delay = std::chrono::milliseconds(i * 2); // 2ms, 4ms, 6ms, 8ms, 10ms
        auto past_time = std::chrono::steady_clock::now() - delay;
        update.timestamp = past_time;
        
        sim_state_->pushUIUpdate(update);
        EXPECT_TRUE(consumer_->consumeUpdate());
    }
    
    // Check metrics
    auto metrics = consumer_->getMetrics();
    EXPECT_EQ(5u, metrics.updates_consumed);
    EXPECT_GT(metrics.avg_latency_ms, 2.0);   // Min latency
    EXPECT_LT(metrics.avg_latency_ms, 10.0);  // Max latency
    EXPECT_GE(metrics.max_latency_ms, 10.0);  // Should capture the 10ms delay
    EXPECT_LE(metrics.min_latency_ms, 3.0);   // Should capture the 2ms delay (+overhead)
}

TEST_F(UIUpdateConsumerTest, ResetMetricsClearsAllData) {
    // Enable push updates
    sim_state_->enablePushUpdates(true);
    
    // Consume some updates
    for (int i = 1; i <= 3; i++) {
        sim_state_->pushUIUpdate(createTestUpdate(i));
        consumer_->consumeUpdate();
    }
    
    // Verify metrics are non-zero
    auto metrics = consumer_->getMetrics();
    EXPECT_GT(metrics.updates_consumed, 0u);
    
    // Reset metrics
    consumer_->resetMetrics();
    
    // Verify metrics are cleared
    metrics = consumer_->getMetrics();
    EXPECT_EQ(0u, metrics.updates_consumed);
    EXPECT_EQ(0u, metrics.updates_missed);
    EXPECT_EQ(0.0, metrics.avg_latency_ms);
    EXPECT_EQ(0.0, metrics.max_latency_ms);
    EXPECT_EQ(std::numeric_limits<double>::max(), metrics.min_latency_ms);
}

TEST_F(UIUpdateConsumerTest, HandlesRapidUpdates) {
    // Enable push updates
    sim_state_->enablePushUpdates(true);
    
    // First consume one update to establish baseline
    sim_state_->pushUIUpdate(createTestUpdate(1));
    EXPECT_TRUE(consumer_->consumeUpdate());
    
    // Now rapidly push many updates (simulating 60fps)
    for (int i = 2; i <= 11; i++) {
        sim_state_->pushUIUpdate(createTestUpdate(i));
        // Only the last one should remain due to latest-update-wins
    }
    
    // Should only consume the latest update (11)
    EXPECT_TRUE(consumer_->consumeUpdate());
    EXPECT_FALSE(consumer_->consumeUpdate()); // Queue should be empty
    
    // Should have missed 9 updates (2-10)
    auto metrics = consumer_->getMetrics();
    
    // Get queue metrics for additional verification
    auto queue_metrics = sim_state_->getUIUpdateMetrics();
    
    EXPECT_EQ(2u, metrics.updates_consumed);  // Consumed 1 and 11
    EXPECT_EQ(9u, metrics.updates_missed);     // Missed 2-10
    
    // Also check queue metrics to verify drops
    EXPECT_EQ(11u, queue_metrics.pushCount);  // Pushed 1-11
    EXPECT_EQ(2u, queue_metrics.popCount);   // Popped 1 and 11
    EXPECT_EQ(9u, queue_metrics.dropCount);  // Dropped 2-10
}