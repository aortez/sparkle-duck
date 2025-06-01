#include "Timers.h"

void Timers::startTimer(const std::string& name) {
    auto& timer = timers[name];
    if (!timer.isRunning) {
        timer.startTime = std::chrono::steady_clock::now();
        timer.isRunning = true;
        timer.callCount++;  // Increment call count when timer starts
    }
}

double Timers::stopTimer(const std::string& name) {
    auto it = timers.find(name);
    if (it == timers.end()) {
        return -1.0; // Timer not found
    }

    auto& timer = it->second;
    if (!timer.isRunning) {
        return timer.accumulatedTime; // Return accumulated time if timer wasn't running
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - timer.startTime);
    timer.accumulatedTime += duration.count();
    timer.isRunning = false;
    return timer.accumulatedTime;
}

bool Timers::hasTimer(const std::string& name) const {
    return timers.find(name) != timers.end();
}

double Timers::getAccumulatedTime(const std::string& name) const {
    auto it = timers.find(name);
    if (it == timers.end()) {
        return -1.0; // Timer not found
    }

    const auto& timer = it->second;
    if (!timer.isRunning) {
        return timer.accumulatedTime;
    }

    // If timer is running, include current session
    auto current = std::chrono::steady_clock::now();
    auto currentDuration = std::chrono::duration_cast<std::chrono::milliseconds>(current - timer.startTime);
    return timer.accumulatedTime + currentDuration.count();
}

void Timers::resetTimer(const std::string& name) {
    auto it = timers.find(name);
    if (it != timers.end()) {
        it->second.accumulatedTime = 0.0;
        if (it->second.isRunning) {
            it->second.startTime = std::chrono::steady_clock::now();
        }
    }
}

uint32_t Timers::getCallCount(const std::string& name) const {
    auto it = timers.find(name);
    if (it == timers.end()) {
        return 0; // Return 0 for non-existent timer
    }
    return it->second.callCount;
}

void Timers::resetCallCount(const std::string& name) {
    auto it = timers.find(name);
    if (it != timers.end()) {
        it->second.callCount = 0;
    }
} 
 