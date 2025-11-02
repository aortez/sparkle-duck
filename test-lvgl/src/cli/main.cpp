#include "WebSocketClient.h"
#include <args.hxx>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include <string>

using namespace DirtSim;

// Command registry for help text and validation.
struct CommandInfo {
    std::string name;
    std::string description;
    std::string example_params;
};

static const std::vector<CommandInfo> AVAILABLE_COMMANDS = {
    {"cell_get", "Get cell state at coordinates", R"({"x": 10, "y": 20})"},
    {"cell_set", "Place material at coordinates", R"({"x": 50, "y": 50, "material": "WATER", "fill": 1.0})"},
    {"diagram_get", "Get emoji visualization of world", ""},
    {"exit", "Shutdown server", ""},
    {"gravity_set", "Set gravity value", R"({"gravity": 15.0})"},
    {"reset", "Reset simulation to initial state", ""},
    {"sim_run", "Start autonomous simulation", R"({"timestep": 0.016, "max_steps": 100})"},
    {"state_get", "Get complete world state as JSON", ""},
};

std::string getCommandListHelp() {
    std::string help = "Available commands:\n";
    for (const auto& cmd : AVAILABLE_COMMANDS) {
        help += "  " + cmd.name + " - " + cmd.description + "\n";
    }
    return help;
}

std::string getExamplesHelp() {
    std::string examples = "Examples:\n";
    for (const auto& cmd : AVAILABLE_COMMANDS) {
        if (!cmd.example_params.empty()) {
            examples += "  cli ws://localhost:8080 " + cmd.name + " '" + cmd.example_params + "'\n";
        } else {
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

    args::Positional<std::string> address(
        parser, "address", "WebSocket URL (e.g., ws://localhost:8080)");
    args::Positional<std::string> command(
        parser,
        "command",
        getCommandListHelp());
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
    if (!address || !command) {
        std::cerr << "Error: address and command are required\n\n";
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

    int timeoutMs = timeout ? args::get(timeout) : 5000;

    // Build command JSON.
    std::string commandJson = buildCommand(
        args::get(command), params ? args::get(params) : "");
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
    std::string commandName = args::get(command);
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
