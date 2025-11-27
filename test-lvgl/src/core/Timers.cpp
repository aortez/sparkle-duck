#include "Timers.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

void Timers::startTimer(const std::string& name)
{
    auto& timer = timers[name];
    if (!timer.isRunning) {
        timer.startTime = std::chrono::steady_clock::now();
        timer.isRunning = true;
        timer.callCount++; // Increment call count when timer starts.
    }
}

double Timers::stopTimer(const std::string& name)
{
    auto it = timers.find(name);
    if (it == timers.end()) {
        return -1.0; // Timer not found.
    }

    auto& timer = it->second;
    if (!timer.isRunning) {
        return timer.accumulatedTime; // Return accumulated time if timer wasn't running.
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - timer.startTime);
    timer.accumulatedTime += duration.count() / 1000.0; // Convert to milliseconds.
    timer.isRunning = false;
    return timer.accumulatedTime;
}

bool Timers::hasTimer(const std::string& name) const
{
    return timers.find(name) != timers.end();
}

double Timers::getAccumulatedTime(const std::string& name) const
{
    auto it = timers.find(name);
    if (it == timers.end()) {
        return -1.0; // Timer not found.
    }

    const auto& timer = it->second;
    if (!timer.isRunning) {
        return timer.accumulatedTime;
    }

    // If timer is running, include current session.
    auto current = std::chrono::steady_clock::now();
    auto currentDuration =
        std::chrono::duration_cast<std::chrono::microseconds>(current - timer.startTime);
    return timer.accumulatedTime + (currentDuration.count() / 1000.0); // Convert to milliseconds.
}

void Timers::resetTimer(const std::string& name)
{
    auto it = timers.find(name);
    if (it != timers.end()) {
        it->second.accumulatedTime = 0.0;
        if (it->second.isRunning) {
            it->second.startTime = std::chrono::steady_clock::now();
        }
    }
}

uint32_t Timers::getCallCount(const std::string& name) const
{
    auto it = timers.find(name);
    if (it == timers.end()) {
        return 0; // Return 0 for non-existent timer.
    }
    return it->second.callCount;
}

void Timers::resetCallCount(const std::string& name)
{
    auto it = timers.find(name);
    if (it != timers.end()) {
        it->second.callCount = 0;
    }
}

void Timers::dumpTimerStats() const
{
    std::cout << "\nTimer Statistics:" << std::endl;
    std::cout << "----------------" << std::endl;

    // Get total simulation time.
    double totalTime = getAccumulatedTime("total_simulation");
    uint32_t totalCalls = getCallCount("total_simulation");
    std::cout << "Total Simulation Time: " << totalTime << "ms ("
              << (totalCalls > 0 ? totalTime / totalCalls : 0) << "ms avg per call, " << totalCalls
              << " calls)" << std::endl;

    // Get advance time stats.
    double advanceTime = getAccumulatedTime("advance_time");
    uint32_t advanceCalls = getCallCount("advance_time");
    std::cout << "Physics Update Time: " << advanceTime << "ms ("
              << (advanceTime / totalTime * 100.0) << "% of total, "
              << (advanceCalls > 0 ? advanceTime / advanceCalls : 0) << "ms avg per call, "
              << advanceCalls << " calls)" << std::endl;

    // Get draw time stats.
    double drawTime = getAccumulatedTime("draw");
    uint32_t drawCalls = getCallCount("draw");
    std::cout << "Drawing Time: " << drawTime << "ms (" << (drawTime / totalTime * 100.0)
              << "% of total, " << (drawCalls > 0 ? drawTime / drawCalls : 0) << "ms avg per call, "
              << drawCalls << " calls)" << std::endl;

    // Get particle addition time if enabled.
    double particleTime = getAccumulatedTime("add_particles");
    if (particleTime > 0) {
        uint32_t particleCalls = getCallCount("add_particles");
        std::cout << "Particle Addition Time: " << particleTime << "ms ("
                  << (particleTime / totalTime * 100.0) << "% of total, "
                  << (particleCalls > 0 ? particleTime / particleCalls : 0) << "ms avg per call, "
                  << particleCalls << " calls)" << std::endl;
    }

    // Get drag processing time.
    double dragTime = getAccumulatedTime("process_drag_end");
    uint32_t dragCalls = getCallCount("process_drag_end");
    std::cout << "Drag Processing Time: " << dragTime << "ms (" << (dragTime / totalTime * 100.0)
              << "% of total, " << (dragCalls > 0 ? dragTime / dragCalls : 0) << "ms avg per call, "
              << dragCalls << " calls)" << std::endl;

    // Physics subsystem breakdown.
    std::cout << "\nPhysics Subsystems:" << std::endl;
    std::cout << "-------------------" << std::endl;

    // List of physics subsystem timers to dump.
    const std::vector<std::string> physicsTimers = {
        "resolve_forces",       "compute_support_map",  "support_calculation",
        "apply_gravity",        "apply_air_resistance", "apply_cohesion_forces",
        "cohesion_calculation", "adhesion_calculation", "apply_pressure_forces",
        "velocity_limiting",    "update_transfers",     "process_moves",
        "hydrostatic_pressure", "dynamic_pressure",     "pressure_diffusion",
        "pressure_decay"
    };

    for (const auto& timerName : physicsTimers) {
        double time = getAccumulatedTime(timerName);
        uint32_t calls = getCallCount(timerName);
        if (time >= 0 || calls > 0) { // Show all timers, even 0ms ones.
            std::cout << "  " << timerName << ": " << time << "ms (" << (time / advanceTime * 100.0)
                      << "% of physics, " << (calls > 0 ? time / calls : 0) << "ms avg, " << calls
                      << " calls)" << std::endl;
        }
    }

    std::cout << "----------------" << std::endl;
}

std::vector<std::string> Timers::getAllTimerNames() const
{
    std::vector<std::string> names;
    names.reserve(timers.size());
    for (const auto& pair : timers) {
        names.push_back(pair.first);
    }
    return names;
}

nlohmann::json Timers::exportAllTimersAsJson() const
{
    nlohmann::json j = nlohmann::json::object();

    for (const auto& [name, timerData] : timers) {
        double total_ms = getAccumulatedTime(name);
        uint32_t calls = timerData.callCount;
        double avg_ms = calls > 0 ? total_ms / calls : 0.0;

        j[name] = { { "total_ms", total_ms }, { "avg_ms", avg_ms }, { "calls", calls } };
    }

    return j;
}
