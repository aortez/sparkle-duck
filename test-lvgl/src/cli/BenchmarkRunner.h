#pragma once

#include "SubprocessManager.h"
#include "core/network/WebSocketService.h"
#include "server/api/TimerStatsGet.h"

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace DirtSim {
namespace Client {

/**
 * @brief Results from benchmark run (flattened for ReflectSerializer).
 */
struct BenchmarkResults {
    std::string scenario = "sandbox";
    std::string grid_size = "28x28";
    uint32_t steps = 0;
    double duration_sec = 0.0;

    // Server metrics.
    double server_fps = 0.0;
    double server_physics_avg_ms = 0.0;
    double server_physics_total_ms = 0.0;
    uint32_t server_physics_calls = 0;
    double server_serialization_avg_ms = 0.0;
    double server_serialization_total_ms = 0.0;
    uint32_t server_serialization_calls = 0;
    double server_cache_update_avg_ms = 0.0;
    double server_network_send_avg_ms = 0.0;

    nlohmann::json timer_stats;
    nlohmann::json final_world_state; // Optional: captured via state_get if requested.
};

/**
 * @brief Runs performance benchmark on Sparkle Duck server.
 *
 * Launches server, runs simulation, collects performance metrics,
 * and outputs JSON results.
 */
class BenchmarkRunner {
public:
    BenchmarkRunner();
    ~BenchmarkRunner();

    BenchmarkResults run(
        const std::string& serverPath,
        uint32_t steps,
        const std::string& scenario = "benchmark",
        int worldSize = 0);

    BenchmarkResults runWithServerArgs(
        const std::string& serverPath,
        uint32_t steps,
        const std::string& scenario,
        const std::string& serverArgs,
        int worldSize = 0);

private:
    SubprocessManager subprocessManager_;
    Network::WebSocketService client_;

    bool waitForCompletion(uint32_t targetSteps, int timeoutSec);

    nlohmann::json queryPerfStats();
};

} // namespace Client
} // namespace DirtSim
