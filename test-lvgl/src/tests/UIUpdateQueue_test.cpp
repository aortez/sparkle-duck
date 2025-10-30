#include "../SharedSimState.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

/**
 * @brief Test suite for UIUpdateQueue thread-safe queue implementation.
 */
class UIUpdateQueueTest : public ::testing::Test {
protected:
    UIUpdateQueue queue;
    
    // Helper to create a test UIUpdateEvent.
    UIUpdateEvent createTestEvent(uint32_t fps = 60) {
        UIUpdateEvent event;
        event.fps = fps;
        event.stepCount = 1000;
        event.isPaused = false;
        event.selectedMaterial = MaterialType::DIRT;
        event.worldType = "World";
        event.timestamp = std::chrono::steady_clock::now();
        return event;
    }
};

TEST_F(UIUpdateQueueTest, BasicPushPop) {
    // Initially empty.
    EXPECT_FALSE(queue.hasPendingUpdate());
    
    // Push an update.
    auto event = createTestEvent(60);
    queue.push(event);
    EXPECT_TRUE(queue.hasPendingUpdate());
    
    // Pop the update.
    auto popped = queue.popLatest();
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(popped->fps, 60);
    EXPECT_FALSE(queue.hasPendingUpdate());
    
    // Queue should be empty now.
    auto empty = queue.popLatest();
    EXPECT_FALSE(empty.has_value());
}

TEST_F(UIUpdateQueueTest, LatestUpdateWins) {
    // Push multiple updates.
    queue.push(createTestEvent(30));
    queue.push(createTestEvent(60));
    queue.push(createTestEvent(120));
    
    // Only the latest should be available.
    auto popped = queue.popLatest();
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(popped->fps, 120);
    
    // Queue should be empty after pop.
    EXPECT_FALSE(queue.hasPendingUpdate());
}

TEST_F(UIUpdateQueueTest, Metrics) {
    auto initialMetrics = queue.getMetrics();
    EXPECT_EQ(initialMetrics.pushCount, 0);
    EXPECT_EQ(initialMetrics.popCount, 0);
    EXPECT_EQ(initialMetrics.dropCount, 0);
    
    // Push 3 updates (2 will be dropped).
    queue.push(createTestEvent(30));
    queue.push(createTestEvent(60));
    queue.push(createTestEvent(90));
    
    // Pop one update.
    queue.popLatest();
    
    auto metrics = queue.getMetrics();
    EXPECT_EQ(metrics.pushCount, 3);
    EXPECT_EQ(metrics.popCount, 1);
    EXPECT_EQ(metrics.dropCount, 2);
}

TEST_F(UIUpdateQueueTest, ThreadSafety) {
    const int numPushThreads = 4;
    const int numPushesPerThread = 1000;
    const int numPopThreads = 2;
    const int numPopsPerThread = 2000;
    
    std::vector<std::thread> threads;
    
    // Start push threads.
    for (int i = 0; i < numPushThreads; ++i) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < numPushesPerThread; ++j) {
                queue.push(createTestEvent(i * 1000 + j));
            }
        });
    }
    
    // Start pop threads.
    for (int i = 0; i < numPopThreads; ++i) {
        threads.emplace_back([this]() {
            for (int j = 0; j < numPopsPerThread; ++j) {
                queue.popLatest();
                // Small delay to make race conditions more likely.
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        });
    }
    
    // Wait for all threads.
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify metrics consistency.
    auto metrics = queue.getMetrics();
    EXPECT_EQ(metrics.pushCount, numPushThreads * numPushesPerThread);
    // Pop count should be at most the number of attempts.
    EXPECT_LE(metrics.popCount, numPopThreads * numPopsPerThread);
    // Drops should be reasonable (many pushes overwrite each other).
    EXPECT_GT(metrics.dropCount, 0);
    
    // Total pushes = successful pushes + drops.
    EXPECT_EQ(metrics.pushCount, metrics.popCount + metrics.dropCount + 
              (queue.hasPendingUpdate() ? 1 : 0));
}

TEST_F(UIUpdateQueueTest, EmptyQueueHandling) {
    // Multiple pops on empty queue should return empty.
    for (int i = 0; i < 10; ++i) {
        auto result = queue.popLatest();
        EXPECT_FALSE(result.has_value());
    }
    
    // Metrics should show pop attempts.
    auto metrics = queue.getMetrics();
    EXPECT_EQ(metrics.popCount, 0); // No successful pops.
    EXPECT_EQ(metrics.pushCount, 0);
    EXPECT_EQ(metrics.dropCount, 0);
}

TEST_F(UIUpdateQueueTest, MoveSemantics) {
    // Create event with specific data.
    UIUpdateEvent event;
    event.fps = 144;
    event.worldType = "TestWorld";
    event.stats.totalCells = 12345;
    
    // Push by move.
    queue.push(std::move(event));
    
    // Pop and verify data.
    auto popped = queue.popLatest();
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(popped->fps, 144);
    EXPECT_EQ(popped->worldType, "TestWorld");
    EXPECT_EQ(popped->stats.totalCells, 12345);
}