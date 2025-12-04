#include "BenchmarkRunner.h"
#include "core/ReflectSerializer.h"
#include "core/WorldData.h"
#include "server/api/Exit.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <numeric>
#include <spdlog/spdlog.h>
#include <thread>

namespace DirtSim {
namespace Client {

BenchmarkRunner::BenchmarkRunner()
{}

BenchmarkRunner::~BenchmarkRunner()
{}

BenchmarkResults BenchmarkRunner::run(
    const std::string& serverPath, uint32_t steps, const std::string& scenario, int worldSize)
{
    BenchmarkResults results;
    results.scenario = scenario;
    results.steps = steps;

    // Launch server with benchmark logging config (logs to file only, console disabled).
    if (!subprocessManager_.launchServer(
            serverPath, "--log-config benchmark-logging-config.json")) {
        spdlog::error("BenchmarkRunner: Failed to launch server");
        return results;
    }

    if (!subprocessManager_.waitForServerReady("ws://localhost:8080", 10)) {
        spdlog::error("BenchmarkRunner: Server failed to start");
        return results;
    }

    auto connectResult = client_.connect("ws://localhost:8080");
    if (connectResult.isError()) {
        spdlog::error(
            "BenchmarkRunner: Failed to connect to server: {}", connectResult.errorValue());
        return results;
    }

    // Start simulation.
    auto benchmarkStart = std::chrono::steady_clock::now();

    nlohmann::json simRunCmd = { { "command", "sim_run" },
                                 { "timestep", 0.016 },
                                 { "max_steps", steps },
                                 { "scenario_id", scenario } };
    auto simRunResult = client_.sendJsonAndReceive(simRunCmd.dump(), 5000);
    if (simRunResult.isError()) {
        spdlog::error("BenchmarkRunner: SimRun failed: {}", simRunResult.errorValue().message);
        return results;
    }
    std::string simRunResponse = simRunResult.value();

    try {
        nlohmann::json json = nlohmann::json::parse(simRunResponse);
        if (json.contains("error")) {
            spdlog::error("BenchmarkRunner: SimRun failed: {}", json["error"].get<std::string>());
            return results;
        }
    }
    catch (const std::exception& e) {
        spdlog::error("BenchmarkRunner: Failed to parse SimRun response: {}", e.what());
        return results;
    }

    spdlog::info("BenchmarkRunner: Started simulation ({} steps, scenario: {})", steps, scenario);

    // Resize world if size specified (must be done after sim_run, when in SimRunning state).
    if (worldSize > 0) {
        spdlog::info("BenchmarkRunner: Resizing world to {}x{}", worldSize, worldSize);
        nlohmann::json resizeCmd = { { "command", "world_resize" },
                                     { "width", worldSize },
                                     { "height", worldSize } };
        auto resizeResult = client_.sendJsonAndReceive(resizeCmd.dump(), 5000);
        if (resizeResult.isError()) {
            spdlog::error(
                "BenchmarkRunner: World resize failed: {}", resizeResult.errorValue().message);
            return results;
        }
        std::string resizeResponse = resizeResult.value();
        try {
            nlohmann::json json = nlohmann::json::parse(resizeResponse);
            if (json.contains("error")) {
                spdlog::error(
                    "BenchmarkRunner: World resize failed: {}", json["error"].get<std::string>());
                return results;
            }
            spdlog::info("BenchmarkRunner: World resized successfully");
        }
        catch (const std::exception& e) {
            spdlog::error("BenchmarkRunner: Failed to parse resize response: {}", e.what());
            return results;
        }
    }

    // Poll state_get until simulation completes.
    int timeoutSec = (steps * 50) / 1000 + 10;
    bool benchmarkComplete = false;

    while (true) {
        // Check if server is still alive.
        if (!subprocessManager_.isServerRunning()) {
            spdlog::error("BenchmarkRunner: Server process died during benchmark!");
            spdlog::error("BenchmarkRunner: Check sparkle-duck.log for crash details");
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // Poll current step using lightweight status_get (not state_get).
        nlohmann::json statusGetCmd = { { "command", "status_get" } };
        auto statusResult = client_.sendJsonAndReceive(statusGetCmd.dump(), 1000);
        if (statusResult.isError()) {
            continue; // Retry on error.
        }
        std::string response = statusResult.value();

        if (response.empty()) {
            // Response timeout or error - continue polling.
            continue;
        }

        try {
            nlohmann::json json = nlohmann::json::parse(response);
            if (json.contains("value")) {
                const auto& value = json["value"];
                if (value.contains("timestep")) {
                    uint64_t step = value["timestep"].get<uint64_t>();

                    // Capture world dimensions on first successful query.
                    if (results.grid_size == "28x28" && value.contains("width")
                        && value.contains("height")) {
                        uint32_t width = value["width"].get<uint32_t>();
                        uint32_t height = value["height"].get<uint32_t>();
                        results.grid_size = std::to_string(width) + "x" + std::to_string(height);
                        spdlog::info("BenchmarkRunner: World size {}x{}", width, height);
                    }

                    if (step >= steps) {
                        spdlog::info(
                            "BenchmarkRunner: Benchmark complete (step {} >= target {})",
                            step,
                            steps);
                        benchmarkComplete = true;
                        break;
                    }
                }
            }
        }
        catch (const std::exception& e) {
            spdlog::debug("BenchmarkRunner: Failed to parse status_get response: {}", e.what());
            // Continue polling.
        }

        // Check timeout.
        auto elapsed = std::chrono::steady_clock::now() - benchmarkStart;
        if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() > timeoutSec) {
            spdlog::error("BenchmarkRunner: Timeout waiting for completion ({}s)", timeoutSec);
            break;
        }
    }

    auto benchmarkEnd = std::chrono::steady_clock::now();
    results.duration_sec = std::chrono::duration<double>(benchmarkEnd - benchmarkStart).count();

    if (!benchmarkComplete) {
        spdlog::error("BenchmarkRunner: Benchmark did not complete");
        client_.disconnect();
        return results;
    }

    // Query performance stats.
    spdlog::info("BenchmarkRunner: Requesting perf_stats from server");
    nlohmann::json perfStatsCmd = { { "command", "perf_stats_get" } };
    auto perfResult = client_.sendJsonAndReceive(perfStatsCmd.dump(), 2000);
    if (perfResult.isError()) {
        spdlog::warn("Failed to get perf stats: {}", perfResult.errorValue().message);
        return results;
    }
    std::string perfStatsResponse = perfResult.value();

    try {
        nlohmann::json perfStatsJson = nlohmann::json::parse(perfStatsResponse);

        if (perfStatsJson.contains("value")) {
            const auto& value = perfStatsJson["value"];

            // Populate server stats.
            results.server_fps = value.value("fps", 0.0);
            results.server_physics_avg_ms = value.value("physics_avg_ms", 0.0);
            results.server_physics_total_ms = value.value("physics_total_ms", 0.0);
            results.server_physics_calls = value.value("physics_calls", 0U);
            results.server_serialization_avg_ms = value.value("serialization_avg_ms", 0.0);
            results.server_serialization_total_ms = value.value("serialization_total_ms", 0.0);
            results.server_serialization_calls = value.value("serialization_calls", 0U);
            results.server_cache_update_avg_ms = value.value("cache_update_avg_ms", 0.0);
            results.server_network_send_avg_ms = value.value("network_send_avg_ms", 0.0);

            spdlog::info(
                "BenchmarkRunner: Server stats - fps: {:.1f}, physics: {:.1f}ms avg, "
                "serialization: {:.1f}ms avg",
                results.server_fps,
                results.server_physics_avg_ms,
                results.server_serialization_avg_ms);
        }
        else {
            spdlog::warn(
                "BenchmarkRunner: perf_stats response missing 'value' field: {}",
                perfStatsJson.dump());
        }
    }
    catch (const std::exception& e) {
        spdlog::error("BenchmarkRunner: Failed to parse perf_stats: {}", e.what());
    }

    // Query detailed timer statistics.
    spdlog::info("BenchmarkRunner: Requesting timer_stats from server");
    nlohmann::json timerStatsCmd = { { "command", "timer_stats_get" } };
    auto timerResult = client_.sendJsonAndReceive(timerStatsCmd.dump(), 2000);
    if (timerResult.isError()) {
        spdlog::warn("Failed to get timer stats: {}", timerResult.errorValue().message);
        return results;
    }
    std::string timerStatsResponse = timerResult.value();

    try {
        nlohmann::json timerStatsJson = nlohmann::json::parse(timerStatsResponse);
        if (timerStatsJson.contains("value")) {
            results.timer_stats = timerStatsJson["value"];
            spdlog::info("BenchmarkRunner: Received {} timer stats", results.timer_stats.size());
        }
    }
    catch (const std::exception& e) {
        spdlog::error("BenchmarkRunner: Failed to parse timer_stats: {}", e.what());
    }

    // Send exit command to cleanly shut down server.
    spdlog::info("BenchmarkRunner: Sending exit command to server");
    nlohmann::json exitCmd = { { "command", "Exit" } };

    try {
        // Use sendAndReceive with short timeout since server may close connection.
        client_.sendJsonAndReceive(exitCmd.dump(), 1000);
    }
    catch (const std::exception& e) {
        // Expected: server closes connection after receiving exit command.
        spdlog::debug("BenchmarkRunner: Exit response: {}", e.what());
    }

    // Note: Client timer stats are not dumped to avoid polluting stdout.
    // Benchmark results already include server timer stats in JSON format.

    // Disconnect and cleanup.
    client_.disconnect();

    return results;
}

BenchmarkResults BenchmarkRunner::runWithServerArgs(
    const std::string& serverPath,
    uint32_t steps,
    const std::string& scenario,
    const std::string& serverArgs,
    int worldSize)
{
    BenchmarkResults results;
    results.scenario = scenario;
    results.steps = steps;

    // Build combined server arguments.
    std::string combinedArgs = "--log-config benchmark-logging-config.json " + serverArgs;

    // Launch server with custom arguments.
    if (!subprocessManager_.launchServer(serverPath, combinedArgs)) {
        spdlog::error("BenchmarkRunner: Failed to launch server with args: {}", serverArgs);
        return results;
    }

    if (!subprocessManager_.waitForServerReady("ws://localhost:8080", 10)) {
        spdlog::error("BenchmarkRunner: Server failed to start");
        return results;
    }

    auto connectResult = client_.connect("ws://localhost:8080");
    if (connectResult.isError()) {
        spdlog::error(
            "BenchmarkRunner: Failed to connect to server: {}", connectResult.errorValue());
        return results;
    }

    // Start simulation.
    auto benchmarkStart = std::chrono::steady_clock::now();

    nlohmann::json simRunCmd = { { "command", "sim_run" },
                                 { "timestep", 0.016 },
                                 { "max_steps", steps },
                                 { "scenario_id", scenario } };
    auto simRunResult = client_.sendJsonAndReceive(simRunCmd.dump(), 5000);
    if (simRunResult.isError()) {
        spdlog::error("BenchmarkRunner: SimRun failed: {}", simRunResult.errorValue().message);
        return results;
    }
    std::string simRunResponse = simRunResult.value();

    try {
        nlohmann::json json = nlohmann::json::parse(simRunResponse);
        if (json.contains("error")) {
            spdlog::error("BenchmarkRunner: SimRun failed: {}", json["error"].get<std::string>());
            return results;
        }
    }
    catch (const std::exception& e) {
        spdlog::error("BenchmarkRunner: Failed to parse SimRun response: {}", e.what());
        return results;
    }

    // Resize world if size specified (must be done after sim_run, when in SimRunning state).
    if (worldSize > 0) {
        spdlog::info("BenchmarkRunner: Resizing world to {}x{}", worldSize, worldSize);
        nlohmann::json resizeCmd = { { "command", "world_resize" },
                                     { "width", worldSize },
                                     { "height", worldSize } };
        auto resizeResult = client_.sendJsonAndReceive(resizeCmd.dump(), 5000);
        if (resizeResult.isError()) {
            spdlog::error(
                "BenchmarkRunner: World resize failed: {}", resizeResult.errorValue().message);
            return results;
        }
        std::string resizeResponse = resizeResult.value();
        try {
            nlohmann::json json = nlohmann::json::parse(resizeResponse);
            if (json.contains("error")) {
                spdlog::error(
                    "BenchmarkRunner: World resize failed: {}", json["error"].get<std::string>());
                return results;
            }
            spdlog::info("BenchmarkRunner: World resized successfully");
        }
        catch (const std::exception& e) {
            spdlog::error("BenchmarkRunner: Failed to parse resize response: {}", e.what());
            return results;
        }
    }

    // Wait for completion (inline polling loop).
    int timeoutSec = (steps * 50) / 1000 + 10;
    bool benchmarkComplete = false;

    while (true) {
        if (!subprocessManager_.isServerRunning()) {
            spdlog::error("BenchmarkRunner: Server died!");
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        nlohmann::json statusCmd = { { "command", "status_get" } };
        auto statusResult = client_.sendJsonAndReceive(statusCmd.dump(), 1000);
        if (statusResult.isError()) {
            continue; // Retry on error.
        }
        std::string response = statusResult.value();

        if (response.empty()) continue;

        try {
            nlohmann::json json = nlohmann::json::parse(response);
            if (json.contains("value") && json["value"].contains("timestep")) {
                uint64_t step = json["value"]["timestep"].get<uint64_t>();

                // Capture grid size.
                if (results.grid_size == "28x28" && json["value"].contains("width")) {
                    uint32_t w = json["value"]["width"].get<uint32_t>();
                    uint32_t h = json["value"]["height"].get<uint32_t>();
                    results.grid_size = std::to_string(w) + "x" + std::to_string(h);
                }

                if (step >= steps) {
                    benchmarkComplete = true;
                    break;
                }
            }
        }
        catch (...) {
        }

        auto elapsed = std::chrono::steady_clock::now() - benchmarkStart;
        if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() > timeoutSec) {
            spdlog::error("BenchmarkRunner: Timeout ({}s)", timeoutSec);
            break;
        }
    }

    auto benchmarkEnd = std::chrono::steady_clock::now();
    results.duration_sec = std::chrono::duration<double>(benchmarkEnd - benchmarkStart).count();

    if (!benchmarkComplete) {
        client_.disconnect();
        return results;
    }

    // Query perf stats.
    nlohmann::json perfStatsJson = queryPerfStats();
    if (!perfStatsJson.empty()) {
        results.server_fps = perfStatsJson.value("fps", 0.0);
        results.server_physics_avg_ms = perfStatsJson.value("physics_avg_ms", 0.0);
        results.server_physics_total_ms = perfStatsJson.value("physics_total_ms", 0.0);
        results.server_physics_calls = perfStatsJson.value("physics_calls", 0U);
        results.server_serialization_avg_ms = perfStatsJson.value("serialization_avg_ms", 0.0);
        results.server_serialization_total_ms = perfStatsJson.value("serialization_total_ms", 0.0);
        results.server_serialization_calls = perfStatsJson.value("serialization_calls", 0U);
        results.server_cache_update_avg_ms = perfStatsJson.value("cache_update_avg_ms", 0.0);
        results.server_network_send_avg_ms = perfStatsJson.value("network_send_avg_ms", 0.0);
    }

    // Query timer stats for detailed breakdown.
    nlohmann::json timerStatsCmd = { { "command", "timer_stats_get" } };
    auto timerResult2 = client_.sendJsonAndReceive(timerStatsCmd.dump(), 2000);
    if (timerResult2.isError()) {
        spdlog::warn("Failed to get timer stats: {}", timerResult2.errorValue().message);
        return results;
    }
    std::string timerResponse = timerResult2.value();

    try {
        nlohmann::json timerJson = nlohmann::json::parse(timerResponse);
        if (timerJson.contains("value")) {
            results.timer_stats = timerJson["value"];
        }
    }
    catch (const std::exception& e) {
        spdlog::error("BenchmarkRunner: Failed to parse timer_stats: {}", e.what());
    }

    // Send exit and disconnect.
    nlohmann::json exitCmd = { { "command", "exit" } };
    auto exitResult = client_.sendJsonAndReceive(exitCmd.dump(), 1000);
    if (exitResult.isError()) {
        spdlog::debug("BenchmarkRunner: Exit failed: {}", exitResult.errorValue().message);
    }

    client_.disconnect();

    return results;
}

nlohmann::json BenchmarkRunner::queryPerfStats()
{
    nlohmann::json cmd = { { "command", "perf_stats_get" } };
    auto perfResult = client_.sendJsonAndReceive(cmd.dump());
    if (perfResult.isError()) {
        spdlog::warn("Failed to query perf stats: {}", perfResult.errorValue().message);
        return nlohmann::json::object();
    }
    std::string response = perfResult.value();

    try {
        nlohmann::json json = nlohmann::json::parse(response);
        if (json.contains("value")) {
            return json["value"];
        }
    }
    catch (const std::exception& e) {
        spdlog::error("BenchmarkRunner: Failed to parse perf_stats: {}", e.what());
    }

    return nlohmann::json::object();
}

} // namespace Client
} // namespace DirtSim
