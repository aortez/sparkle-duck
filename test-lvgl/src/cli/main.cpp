#include "BenchmarkRunner.h"
#include "WebSocketClient.h"
#include "core/ReflectSerializer.h"
#include <args.hxx>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>

using namespace DirtSim;

// Command registry for help text and validation.
struct CommandInfo {
    std::string name;
    std::string description;
    std::string example_params;
};

static const std::vector<CommandInfo> AVAILABLE_COMMANDS = {
    { "benchmark", "Run performance benchmark (launches server)", "" },
    { "cell_get", "Get cell state at coordinates", R"({"x": 10, "y": 20})" },
    { "cell_set",
      "Place material at coordinates",
      R"({"x": 50, "y": 50, "material": "WATER", "fill": 1.0})" },
    { "diagram_get", "Get emoji visualization of world", "" },
    { "exit", "Shutdown server", "" },
    { "gravity_set", "Set gravity value", R"({"gravity": 15.0})" },
    { "perf_stats_get", "Get server performance statistics", "" },
    { "physics_settings_get", "Get current physics settings", "" },
    { "physics_settings_set", "Set physics parameters", R"({"settings": {"timescale": 1.5, "gravity": 0.8}})" },
    { "reset", "Reset simulation to initial state", "" },
    { "scenario_config_set",
      "Update scenario configuration",
      R"({"config": {"type": "sandbox", "quadrant_enabled": true, "water_column_enabled": true, "right_throw_enabled": true, "top_drop_enabled": true, "rain_rate": 0.0}})" },
    { "sim_run", "Start autonomous simulation", R"({"timestep": 0.016, "max_steps": 100})" },
    { "state_get", "Get complete world state as JSON", "" },
    { "timer_stats_get", "Get detailed physics timing breakdown", "" },
    { "step_n", "Advance simulation N frames", R"({"frames": 1})" },
};

std::string getCommandListHelp()
{
    std::string help = "Available commands:\n";
    for (const auto& cmd : AVAILABLE_COMMANDS) {
        help += "  " + cmd.name + " - " + cmd.description + "\n";
    }
    return help;
}

std::string getExamplesHelp()
{
    std::string examples = "Examples:\n";
    for (const auto& cmd : AVAILABLE_COMMANDS) {
        if (!cmd.example_params.empty()) {
            examples += "  cli ws://localhost:8080 " + cmd.name + " '" + cmd.example_params + "'\n";
        }
        else {
            examples += "  cli ws://localhost:8080 " + cmd.name + "\n";
        }
    }
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
        "Benchmark: scenario name (default: sandbox)",
        { "scenario" },
        "sandbox");
    args::Flag simulateUI(
        parser, "simulate-ui", "Benchmark: simulate UI client behavior", { "simulate-ui" });

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
        // Find server binary (assume it's in same directory as CLI).
        std::filesystem::path exePath = std::filesystem::read_symlink("/proc/self/exe");
        std::filesystem::path binDir = exePath.parent_path();
        std::filesystem::path serverPath = binDir / "sparkle-duck-server";

        if (!std::filesystem::exists(serverPath)) {
            std::cerr << "Error: Cannot find server binary at " << serverPath << std::endl;
            return 1;
        }

        // Run benchmark.
        Client::BenchmarkRunner runner;
        auto results = runner.run(
            serverPath.string(), args::get(benchSteps), args::get(benchScenario), simulateUI);

        // Output results as JSON using ReflectSerializer.
        nlohmann::json resultJson = ReflectSerializer::to_json(results);

        // Add timer_stats (already in JSON format).
        if (!results.timer_stats.empty()) {
            resultJson["timer_stats"] = results.timer_stats;
        }

        std::cout << resultJson.dump(2) << std::endl;
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
