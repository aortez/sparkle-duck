#include "core/Timers.h"
#include <cassert>
#include <gtest/gtest.h>
#include <iostream>
#include <spdlog/spdlog.h>
#include <thread>

TEST(TimersTest, BasicTimer)
{
    spdlog::info("Starting TimersTest::BasicTimer test");
    Timers timers;

    // Test starting a timer.
    timers.startTimer("test1");
    EXPECT_TRUE(timers.hasTimer("test1"));

    // Test stopping a timer.
    double elapsed = timers.stopTimer("test1");
    EXPECT_GE(elapsed, 0.0);               // Should be non-negative.
    EXPECT_TRUE(timers.hasTimer("test1")); // Timer should still exist after stopping.

    // Test stopping non-existent timer.
    elapsed = timers.stopTimer("nonexistent");
    EXPECT_EQ(elapsed, -1.0); // Should return -1 for non-existent timer.
}

TEST(TimersTest, MultipleTimers)
{
    spdlog::info("Starting TimersTest::MultipleTimers test");
    Timers timers;

    // Start multiple timers.
    timers.startTimer("timer1");
    timers.startTimer("timer2");

    EXPECT_TRUE(timers.hasTimer("timer1"));
    EXPECT_TRUE(timers.hasTimer("timer2"));

    // Stop one timer.
    double elapsed1 = timers.stopTimer("timer1");
    EXPECT_GE(elapsed1, 0.0);
    EXPECT_TRUE(timers.hasTimer("timer1"));
    EXPECT_TRUE(timers.hasTimer("timer2"));

    // Stop the other timer.
    double elapsed2 = timers.stopTimer("timer2");
    EXPECT_GE(elapsed2, 0.0);
    EXPECT_TRUE(timers.hasTimer("timer2"));
}

TEST(TimersTest, TimerDuration)
{
    spdlog::info("Starting TimersTest::TimerDuration test");
    Timers timers;

    // Start a timer.
    timers.startTimer("duration_test");

    // Sleep for 100ms.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop timer and check duration.
    double elapsed = timers.stopTimer("duration_test");
    EXPECT_GE(elapsed, 100.0); // Should be at least 100ms.
    EXPECT_LT(elapsed, 200.0); // Should be less than 200ms (allowing for some overhead).
}

TEST(TimersTest, CumulativeTiming)
{
    spdlog::info("Starting TimersTest::CumulativeTiming test");
    Timers timers;

    // Start timer.
    timers.startTimer("cumulative_test");

    // Sleep for 100ms.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop timer.
    double firstElapsed = timers.stopTimer("cumulative_test");
    EXPECT_GE(firstElapsed, 100.0);
    EXPECT_LT(firstElapsed, 200.0);

    // Start timer again.
    timers.startTimer("cumulative_test");

    // Sleep for another 100ms.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop timer and check cumulative time.
    double secondElapsed = timers.stopTimer("cumulative_test");
    EXPECT_GE(secondElapsed, 200.0); // Should be at least 200ms total.
    EXPECT_LT(secondElapsed, 300.0); // Should be less than 300ms total.

    // Check accumulated time directly.
    double accumulated = timers.getAccumulatedTime("cumulative_test");
    EXPECT_EQ(accumulated, secondElapsed);
}

TEST(TimersTest, ResetTimer)
{
    spdlog::info("Starting TimersTest::ResetTimer test");
    Timers timers;

    // Start and run timer.
    timers.startTimer("reset_test");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    timers.stopTimer("reset_test");

    // Reset timer.
    timers.resetTimer("reset_test");
    EXPECT_EQ(timers.getAccumulatedTime("reset_test"), 0.0);

    // Start timer again.
    timers.startTimer("reset_test");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    double elapsed = timers.stopTimer("reset_test");
    EXPECT_GE(elapsed, 100.0);
    EXPECT_LT(elapsed, 200.0);
}
