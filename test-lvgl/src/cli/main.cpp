#include "WebSocketClient.h"
#include <args.hxx>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include <string>

using namespace DirtSim;

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
        "Send commands to Sparkle Duck server or UI via WebSocket.\n\n"
        "Examples:\n"
        "  cli ws://localhost:8080 get_state\n"
        "  cli ws://localhost:8080 step '{\"frames\": 100}'\n"
        "  cli ws://localhost:8080 get_cell '{\"x\": 10, \"y\": 20}'\n"
        "  cli ws://localhost:8080 place_material '{\"x\": 50, \"y\": 50, \"material\": \"WATER\", \"fill\": 1.0}'\n"
        "  cli ws://localhost:8080 set_gravity '{\"value\": 15.0}'\n"
        "  cli ws://localhost:8080 reset");

    args::HelpFlag help(parser, "help", "Display this help menu", { 'h', "help" });
    args::Flag verbose(parser, "verbose", "Enable debug logging", { 'v', "verbose" });
    args::ValueFlag<int> timeout(
        parser, "timeout", "Response timeout in milliseconds (default: 5000)", { 't', "timeout" });

    args::Positional<std::string> address(
        parser, "address", "WebSocket URL (e.g., ws://localhost:8080)");
    args::Positional<std::string> command(
        parser,
        "command",
        "Command name: step, get_cell, place_material, get_state, set_gravity, reset");
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

    // Output response to stdout.
    std::cout << response << std::endl;

    client.disconnect();
    return 0;
}
