#include "../src/Timers.h"
#include <cassert>
#include <thread>
#include <iostream>

void testBasicTimer() {
    Timers timers;
    
    // Test starting a timer
    timers.startTimer("test1");
    assert(timers.hasTimer("test1"));
    
    // Test stopping a timer
    double elapsed = timers.stopTimer("test1");
    assert(elapsed >= 0.0); // Should be non-negative
    assert(timers.hasTimer("test1")); // Timer should still exist after stopping
    
    // Test stopping non-existent timer
    elapsed = timers.stopTimer("nonexistent");
    assert(elapsed == -1.0); // Should return -1 for non-existent timer
}

void testMultipleTimers() {
    Timers timers;
    
    // Start multiple timers
    timers.startTimer("timer1");
    timers.startTimer("timer2");
    
    assert(timers.hasTimer("timer1"));
    assert(timers.hasTimer("timer2"));
    
    // Stop one timer
    double elapsed1 = timers.stopTimer("timer1");
    assert(elapsed1 >= 0.0);
    assert(timers.hasTimer("timer1"));
    assert(timers.hasTimer("timer2"));
    
    // Stop the other timer
    double elapsed2 = timers.stopTimer("timer2");
    assert(elapsed2 >= 0.0);
    assert(timers.hasTimer("timer2"));
}

void testTimerDuration() {
    Timers timers;
    
    // Start a timer
    timers.startTimer("duration_test");
    
    // Sleep for 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Stop timer and check duration
    double elapsed = timers.stopTimer("duration_test");
    assert(elapsed >= 100.0); // Should be at least 100ms
    assert(elapsed < 200.0);  // Should be less than 200ms (allowing for some overhead)
}

void testCumulativeTiming() {
    Timers timers;
    
    // Start timer
    timers.startTimer("cumulative_test");
    
    // Sleep for 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Stop timer
    double firstElapsed = timers.stopTimer("cumulative_test");
    assert(firstElapsed >= 100.0);
    assert(firstElapsed < 200.0);
    
    // Start timer again
    timers.startTimer("cumulative_test");
    
    // Sleep for another 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Stop timer and check cumulative time
    double secondElapsed = timers.stopTimer("cumulative_test");
    assert(secondElapsed >= 200.0); // Should be at least 200ms total
    assert(secondElapsed < 300.0);  // Should be less than 300ms total
    
    // Check accumulated time directly
    double accumulated = timers.getAccumulatedTime("cumulative_test");
    assert(accumulated == secondElapsed);
}

void testResetTimer() {
    Timers timers;
    
    // Start and run timer
    timers.startTimer("reset_test");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    timers.stopTimer("reset_test");
    
    // Reset timer
    timers.resetTimer("reset_test");
    assert(timers.getAccumulatedTime("reset_test") == 0.0);
    
    // Start timer again
    timers.startTimer("reset_test");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    double elapsed = timers.stopTimer("reset_test");
    assert(elapsed >= 100.0);
    assert(elapsed < 200.0);
}

int main() {
    std::cout << "Running Timers tests..." << std::endl;
    
    testBasicTimer();
    testMultipleTimers();
    testTimerDuration();
    testCumulativeTiming();
    testResetTimer();
    
    std::cout << "All tests passed!" << std::endl;
    return 0;
} 
 