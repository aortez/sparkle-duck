#pragma once

#include <string>
#include <unordered_map>
#include <chrono>

class Timers {
public:
    Timers() = default;
    ~Timers() = default;

    // Start a timer with the given name
    void startTimer(const std::string& name);

    // Stop a timer with the given name and return elapsed time in milliseconds
    double stopTimer(const std::string& name);

    // Check if a timer exists
    bool hasTimer(const std::string& name) const;

    // Get the total accumulated time for a timer in milliseconds
    double getAccumulatedTime(const std::string& name) const;

    // Reset a timer's accumulated time to 0
    void resetTimer(const std::string& name);

    // Get the number of times a timer has been called
    uint32_t getCallCount(const std::string& name) const;

    // Reset a timer's call count to 0
    void resetCallCount(const std::string& name);

    // Dump statistics for all timers
    void dumpTimerStats() const;

private:
    using TimePoint = std::chrono::steady_clock::time_point;
    
    struct TimerData {
        TimePoint startTime;
        double accumulatedTime = 0.0;
        bool isRunning = false;
        uint32_t callCount = 0;  // Track number of times timer has been called
    };
    
    std::unordered_map<std::string, TimerData> timers;
}; 
 