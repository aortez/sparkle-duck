#include "State.h"
#include "core/Timers.h"
#include "server/StateMachine.h"
#include "server/api/TimerStatsGet.h"
#include "server/scenarios/ScenarioRegistry.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Server {
namespace State {

void SimPaused::onEnter(StateMachine& /*dsm*/)
{
    spdlog::info(
        "SimPaused: Simulation paused at step {} (World preserved)", previousState.stepCount);
}

void SimPaused::onExit(StateMachine& /*dsm*/)
{
    spdlog::info("SimPaused: Exiting paused state");
}

State::Any SimPaused::onEvent(const Api::Exit::Cwc& cwc, StateMachine& /*dsm*/)
{
    spdlog::info("SimPaused: Exit command received, shutting down");

    // Send success response.
    cwc.sendResponse(Api::Exit::Response::okay(std::monostate{}));

    // Transition to Shutdown state (World destroyed with SimPaused).
    return Shutdown{};
}

State::Any SimPaused::onEvent(const Api::StateGet::Cwc& cwc, StateMachine& dsm)
{
    using Response = Api::StateGet::Response;

    if (!previousState.world) {
        cwc.sendResponse(Response::error(ApiError("No world available")));
        return std::move(*this);
    }

    // Return cached WorldData from paused state.
    auto cachedPtr = dsm.getCachedWorldData();
    if (cachedPtr) {
        Api::StateGet::Okay responseData;
        responseData.worldData = *cachedPtr;
        cwc.sendResponse(Response::okay(std::move(responseData)));
    }
    else {
        // Fallback: cache not ready yet, copy from world.
        Api::StateGet::Okay responseData;
        responseData.worldData = previousState.world->getData();
        cwc.sendResponse(Response::okay(std::move(responseData)));
    }
    return std::move(*this);
}

State::Any SimPaused::onEvent(const Api::PerfStatsGet::Cwc& cwc, StateMachine& dsm)
{
    using Response = Api::PerfStatsGet::Response;

    // Gather performance statistics from timers.
    auto& timers = dsm.getTimers();

    Api::PerfStatsGet::Okay stats;
    stats.fps = previousState.actualFPS;

    // Physics timing.
    stats.physics_calls = timers.getCallCount("physics_step");
    stats.physics_total_ms = timers.getAccumulatedTime("physics_step");
    stats.physics_avg_ms =
        stats.physics_calls > 0 ? stats.physics_total_ms / stats.physics_calls : 0.0;

    // Serialization timing.
    stats.serialization_calls = timers.getCallCount("serialize_worlddata");
    stats.serialization_total_ms = timers.getAccumulatedTime("serialize_worlddata");
    stats.serialization_avg_ms = stats.serialization_calls > 0
        ? stats.serialization_total_ms / stats.serialization_calls
        : 0.0;

    // Cache update timing.
    stats.cache_update_calls = timers.getCallCount("cache_update");
    stats.cache_update_total_ms = timers.getAccumulatedTime("cache_update");
    stats.cache_update_avg_ms =
        stats.cache_update_calls > 0 ? stats.cache_update_total_ms / stats.cache_update_calls : 0.0;

    // Network send timing.
    stats.network_send_calls = timers.getCallCount("network_send");
    stats.network_send_total_ms = timers.getAccumulatedTime("network_send");
    stats.network_send_avg_ms =
        stats.network_send_calls > 0 ? stats.network_send_total_ms / stats.network_send_calls : 0.0;

    spdlog::info(
        "SimPaused: API perf_stats_get returning {} physics steps, {} serializations",
        stats.physics_calls,
        stats.serialization_calls);

    cwc.sendResponse(Response::okay(std::move(stats)));
    return std::move(*this);
}

State::Any SimPaused::onEvent(const Api::TimerStatsGet::Cwc& cwc, StateMachine& dsm)
{
    using Response = Api::TimerStatsGet::Response;

    auto& timers = dsm.getTimers();
    std::vector<std::string> timerNames = timers.getAllTimerNames();

    Api::TimerStatsGet::Okay okay;

    for (const auto& name : timerNames) {
        Api::TimerStatsGet::TimerEntry entry;
        entry.total_ms = timers.getAccumulatedTime(name);
        entry.calls = timers.getCallCount(name);
        entry.avg_ms = entry.calls > 0 ? entry.total_ms / entry.calls : 0.0;
        okay.timers[name] = entry;
    }

    spdlog::info("SimPaused: API timer_stats_get returning {} timers", okay.timers.size());

    cwc.sendResponse(Response::okay(std::move(okay)));
    return std::move(*this);
}

// Future handlers:
// - SimRun: Resume with new parameters → std::move(previousState)
// - SimStop: Destroy world and return to Idle

} // namespace State
} // namespace Server
} // namespace DirtSim
