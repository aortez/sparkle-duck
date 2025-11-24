#include "BenchmarkRunner.h"
#include "CleanupRunner.h"
#include "CommandRegistry.h"
#include "IntegrationTest.h"
#include "RunAllRunner.h"
#include "WebSocketClient.h"
#include "core/ReflectSerializer.h"
#include <args.hxx>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>

using namespace DirtSim;

// Helper function to sort timer_stats by total_ms in descending order.
// Returns an array of objects (to preserve sort order) instead of a JSON object.
nlohmann::json sortTimerStats(const nlohmann::json& timer_stats)
{
    if (timer_stats.empty()) {
        return nlohmann::json::array();
    }

    // Convert to vector of pairs for sorting.
    std::vector<std::pair<std::string, nlohmann::json>> timer_pairs;
    for (auto it = timer_stats.begin(); it != timer_stats.end(); ++it) {
        timer_pairs.push_back({ it.key(), it.value() });
    }

    // Sort by total_ms descending.
    std::sort(timer_pairs.begin(), timer_pairs.end(), [](const auto& a, const auto& b) {
        double a_total = a.second.value("total_ms", 0.0);
        double b_total = b.second.value("total_ms", 0.0);
        return a_total > b_total;
    });

    // Build as array of objects with "name" field to preserve order.
    nlohmann::json sorted_timers = nlohmann::json::array();
    for (const auto& pair : timer_pairs) {
        nlohmann::json entry = pair.second;
        entry["name"] = pair.first;
        sorted_timers.push_back(entry);
    }

    return sorted_timers;
}

// CLI-specific commands (not server/UI API commands).
struct CliCommandInfo {
    std::string name;
    std::string description;
};

static const std::vector<CliCommandInfo> CLI_COMMANDS = {
    { "benchmark", "Run performance benchmark (launches server)" },
    { "cleanup", "Clean up rogue sparkle-duck processes" },
    { "integration_test", "Run integration test (launches server + UI)" },
    { "run-all", "Launch server + UI and monitor (exits when UI closes)" },
};

std::string getCommandListHelp()
{
    std::string help = "Available commands:\n\n";

    // CLI-specific commands.
    help += "CLI Commands:\n";
    for (const auto& cmd : CLI_COMMANDS) {
        help += "  " + cmd.name + " - " + cmd.description + "\n";
    }

    // Auto-generated server API commands.
    help += "\nServer API Commands (ws://localhost:8080):\n";
    for (const auto& cmdName : Client::SERVER_COMMAND_NAMES) {
        help += "  " + std::string(cmdName) + "\n";
    }

    // Auto-generated UI API commands.
    help += "\nUI API Commands (ws://localhost:7070):\n";
    for (const auto& cmdName : Client::UI_COMMAND_NAMES) {
        help += "  " + std::string(cmdName) + "\n";
    }

    return help;
}

std::string getExamplesHelp()
{
    std::string examples = "Examples:\n\n";

    // CLI-specific examples.
    examples += "CLI Commands:\n";
    for (const auto& cmd : CLI_COMMANDS) {
        examples += "  cli " + cmd.name + "\n";
    }

    // Server API examples (show a few common ones).
    examples += "\nServer API Examples:\n";
    examples += "  cli ws://localhost:8080 state_get\n";
    examples += "  cli ws://localhost:8080 sim_run '{\"timestep\": 0.016, \"max_steps\": 100}'\n";
    examples += "  cli ws://localhost:8080 cell_set '{\"x\": 50, \"y\": 50, \"material\": "
                "\"WATER\", \"fill\": 1.0}'\n";
    examples += "  cli ws://localhost:8080 diagram_get\n";

    // UI API examples.
    examples += "\nUI API Examples:\n";
    examples += "  cli ws://localhost:7070 draw_debug_toggle '{\"enabled\": true}'\n";
    examples += "  cli ws://localhost:7070 screenshot\n";

    return examples;
}

std::string buildCommand(const std::string& commandName, const std::string& jsonParams)
{
    nlohmann::json cmd;
    cmd["command"] = commandName;

    // If params provided, merge them in.
    if (!jsonParams.empty()) {
        try {
            nlohmann::json params = nlohmann::json::parse(jsonParams);
            // Merge params into cmd.
            for (auto& [key, value] : params.items()) {
                cmd[key] = value;
            }
        }
        catch (const nlohmann::json::parse_error& e) {
            std::cerr << "Error parsing JSON parameters: " << e.what() << std::endl;
            return "";
        }
    }

    return cmd.dump();
}

int main(int argc, char** argv)
{
    // Configure spdlog to output to stderr (stdout reserved for JSON output).
    auto logger = spdlog::stderr_color_mt("cli");
    spdlog::set_default_logger(logger);

    // Parse command line arguments.
    args::ArgumentParser parser(
        "Sparkle Duck CLI Client",
        "Send commands to Sparkle Duck server or UI via WebSocket.\n\n" + getExamplesHelp());

    args::HelpFlag help(parser, "help", "Display this help menu", { 'h', "help" });
    args::Flag verbose(parser, "verbose", "Enable debug logging", { 'v', "verbose" });
    args::ValueFlag<int> timeout(
        parser, "timeout", "Response timeout in milliseconds (default: 5000)", { 't', "timeout" });

    // Benchmark-specific flags.
    args::ValueFlag<int> benchSteps(
        parser, "steps", "Benchmark: number of simulation steps (default: 120)", { "steps" }, 120);
    args::ValueFlag<std::string> benchScenario(
        parser,
        "scenario",
        "Benchmark: scenario name (default: benchmark)",
        { "scenario" },
        "benchmark");
    args::ValueFlag<int> benchWorldSize(
        parser,
        "size",
        "Benchmark: world grid size (default: scenario default)",
        { "world-size", "size" });
    args::Flag compareCache(
        parser,
        "compare-cache",
        "Benchmark: Run twice to compare cached vs non-cached performance",
        { "compare-cache" });

    args::Positional<std::string> command(parser, "command", getCommandListHelp());
    args::Positional<std::string> address(
        parser, "address", "WebSocket URL (e.g., ws://localhost:8080) - not needed for benchmark");
    args::Positional<std::string> params(
        parser, "params", "Optional JSON object with command parameters");

    try {
        parser.ParseCLI(argc, argv);
    }
    catch (const args::Help&) {
        std::cout << parser;
        return 0;
    }
    catch (const args::ParseError& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }

    // Validate required arguments.
    if (!command) {
        std::cerr << "Error: command is required\n\n";
        std::cerr << parser;
        return 1;
    }

    // Configure logging.
    if (verbose) {
        spdlog::set_level(spdlog::level::debug);
    }
    else {
        spdlog::set_level(spdlog::level::err);
    }

    std::string commandName = args::get(command);

    // Handle benchmark command (auto-launches server).
    if (commandName == "benchmark") {
        // Set log level to error for clean JSON output (unless --verbose).
        if (!verbose) {
            spdlog::set_level(spdlog::level::err);
        }

        // Find server binary (assume it's in same directory as CLI).
        std::filesystem::path exePath = std::filesystem::read_symlink("/proc/self/exe");
        std::filesystem::path binDir = exePath.parent_path();
        std::filesystem::path serverPath = binDir / "sparkle-duck-server";

        if (!std::filesystem::exists(serverPath)) {
            std::cerr << "Error: Cannot find server binary at " << serverPath << std::endl;
            return 1;
        }

        // Run benchmark.
        // Follow args library pattern: check presence before get.
        int actualSteps = benchSteps ? args::get(benchSteps) : 120;
        std::string actualScenario = benchScenario ? args::get(benchScenario) : "benchmark";
        (void)benchWorldSize; // Unused - world size now determined by scenario.

        if (compareCache) {
            // Compare full system (cache + OpenMP) vs baseline.
            Client::BenchmarkRunner runner;

            spdlog::set_level(spdlog::level::info);
            spdlog::info("Running benchmark WITH cache + OpenMP (default)...");
            auto results_cached = runner.run(serverPath.string(), actualSteps, actualScenario, 0);

            spdlog::info("Running benchmark WITHOUT cache or OpenMP (baseline)...");
            auto results_direct = runner.runWithServerArgs(
                serverPath.string(), actualSteps, actualScenario, "--no-grid-cache --no-openmp", 0);

            // Build comparison output.
            nlohmann::json comparison;
            comparison["scenario"] = actualScenario;
            comparison["steps"] = actualSteps;

            // Serialize results and sort timer_stats.
            nlohmann::json cached_json = ReflectSerializer::to_json(results_cached);
            if (!results_cached.timer_stats.empty()) {
                cached_json["timer_stats"] = sortTimerStats(results_cached.timer_stats);
            }

            nlohmann::json direct_json = ReflectSerializer::to_json(results_direct);
            if (!results_direct.timer_stats.empty()) {
                direct_json["timer_stats"] = sortTimerStats(results_direct.timer_stats);
            }

            comparison["with_cache_and_openmp"] = cached_json;
            comparison["without_cache_or_openmp_baseline"] = direct_json;

            // Calculate speedup.
            double cached_fps = results_cached.server_fps;
            double direct_fps = results_direct.server_fps;
            double speedup = (cached_fps / direct_fps - 1.0) * 100.0;

            comparison["speedup_percent"] = speedup;

            std::cout << comparison.dump(2) << std::endl;

            return 0;
        }
        else {
            // Single run (default behavior).
            Client::BenchmarkRunner runner;
            auto results = runner.run(serverPath.string(), actualSteps, actualScenario);

            // Output results as JSON using ReflectSerializer.
            nlohmann::json resultJson = ReflectSerializer::to_json(results);

            // Add timer_stats (already in JSON format), sorted by total_ms descending.
            if (!results.timer_stats.empty()) {
                resultJson["timer_stats"] = sortTimerStats(results.timer_stats);
            }

            std::cout << resultJson.dump(2) << std::endl;
        }
        return 0;
    }

    // Handle cleanup command (find and kill rogue processes).
    if (commandName == "cleanup") {
        // Always show cleanup output (unless explicitly verbose).
        if (!verbose) {
            spdlog::set_level(spdlog::level::info);
        }

        Client::CleanupRunner cleanup;
        auto results = cleanup.run();
        return results.empty() ? 0 : 0; // Always return 0 on success.
    }

    // Handle integration_test command (auto-launches server and UI).
    if (commandName == "integration_test") {
        // Find server and UI binaries (assume they're in same directory as CLI).
        std::filesystem::path exePath = std::filesystem::read_symlink("/proc/self/exe");
        std::filesystem::path binDir = exePath.parent_path();
        std::filesystem::path serverPath = binDir / "sparkle-duck-server";
        std::filesystem::path uiPath = binDir / "sparkle-duck-ui";

        if (!std::filesystem::exists(serverPath)) {
            std::cerr << "Error: Cannot find server binary at " << serverPath << std::endl;
            return 1;
        }

        if (!std::filesystem::exists(uiPath)) {
            std::cerr << "Error: Cannot find UI binary at " << uiPath << std::endl;
            return 1;
        }

        // Run integration test.
        Client::IntegrationTest test;
        return test.run(serverPath.string(), uiPath.string());
    }

    // Handle run-all command (launches server and UI, monitors until UI exits).
    if (commandName == "run-all") {
        // Find server and UI binaries (assume they're in same directory as CLI).
        std::filesystem::path exePath = std::filesystem::read_symlink("/proc/self/exe");
        std::filesystem::path binDir = exePath.parent_path();
        std::filesystem::path serverPath = binDir / "sparkle-duck-server";
        std::filesystem::path uiPath = binDir / "sparkle-duck-ui";

        if (!std::filesystem::exists(serverPath)) {
            std::cerr << "Error: Cannot find server binary at " << serverPath << std::endl;
            return 1;
        }

        if (!std::filesystem::exists(uiPath)) {
            std::cerr << "Error: Cannot find UI binary at " << uiPath << std::endl;
            return 1;
        }

        // Run server and UI.
        auto result = Client::runAll(serverPath.string(), uiPath.string());
        if (result.isError()) {
            std::cerr << "Error: " << result.errorValue() << std::endl;
            return 1;
        }
        return 0;
    }

    // Normal command mode - require address.
    if (!address) {
        std::cerr << "Error: address is required for non-benchmark commands\n\n";
        std::cerr << parser;
        return 1;
    }

    int timeoutMs = timeout ? args::get(timeout) : 5000;

    // Build command JSON.
    std::string commandJson = buildCommand(args::get(command), params ? args::get(params) : "");
    if (commandJson.empty()) {
        return 1;
    }

    // Connect to server.
    Client::WebSocketClient client;
    if (!client.connect(args::get(address))) {
        std::cerr << "Failed to connect to " << args::get(address) << std::endl;
        return 1;
    }

    // Send command and receive response.
    std::string response = client.sendAndReceive(commandJson, timeoutMs);
    if (response.empty()) {
        std::cerr << "Failed to receive response" << std::endl;
        return 1;
    }

    // Special handling for diagram_get - extract and display just the diagram.
    if (commandName == "diagram_get") {
        try {
            nlohmann::json responseJson = nlohmann::json::parse(response);
            spdlog::debug("Parsed response JSON: {}", responseJson.dump(2));

            if (responseJson.contains("value") && responseJson["value"].contains("diagram")) {
                std::cout << responseJson["value"]["diagram"].get<std::string>() << std::endl;
            }
            else {
                // Fallback: display raw response.
                spdlog::warn("Response doesn't contain expected diagram structure");
                std::cout << response << std::endl;
            }
        }
        catch (const nlohmann::json::parse_error& e) {
            spdlog::error("JSON parse error: {}", e.what());
            std::cout << response << std::endl;
        }
    }
    else {
        // Output response to stdout.
        std::cout << response << std::endl;
    }

    client.disconnect();
    return 0;
}
