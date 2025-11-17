#include "StateMachine.h"
#include "network/WebSocketServer.h"
#include <args.hxx>
#include <csignal>
#include <memory>
#include <spdlog/spdlog.h>

using namespace DirtSim;

// Global pointer for signal handler.
static Server::StateMachine* g_stateMachine = nullptr;

void signalHandler(int signum)
{
    spdlog::info("Interrupt signal ({}) received, shutting down...", signum);
    if (g_stateMachine) {
        g_stateMachine->setShouldExit(true);
    }
}

int main(int argc, char** argv)
{
    // Parse command line arguments.
    args::ArgumentParser parser(
        "Sparkle Duck WebSocket Server", "Remote simulation control via WebSocket.");
    args::HelpFlag help(parser, "help", "Display this help menu", { 'h', "help" });
    args::ValueFlag<uint16_t> portArg(
        parser, "port", "WebSocket port (default: 8080)", { 'p', "port" });
    args::ValueFlag<int> stepsArg(
        parser,
        "steps",
        "Number of simulation steps to run (default: unlimited)",
        { 's', "steps" });
    args::ValueFlag<std::string> logLevel(
        parser,
        "log-level",
        "Set log level (trace, debug, info, warn, error, critical, off)",
        { 'l', "log-level" },
        "info");
    args::Flag printStats(
        parser, "print-stats", "Print timer statistics on exit", { "print-stats" });

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

    uint16_t port = portArg ? args::get(portArg) : 8080;
    int maxSteps = stepsArg ? args::get(stepsArg) : -1;

    // Configure logging based on the requested log level.
    spdlog::level::level_enum selectedLevel = spdlog::level::info; // Default.
    if (logLevel) {
        std::string levelStr = args::get(logLevel);
        if (levelStr == "trace") {
            selectedLevel = spdlog::level::trace;
        }
        else if (levelStr == "debug") {
            selectedLevel = spdlog::level::debug;
        }
        else if (levelStr == "info") {
            selectedLevel = spdlog::level::info;
        }
        else if (levelStr == "warn" || levelStr == "warning") {
            selectedLevel = spdlog::level::warn;
        }
        else if (levelStr == "error" || levelStr == "err") {
            selectedLevel = spdlog::level::err;
        }
        else if (levelStr == "critical") {
            selectedLevel = spdlog::level::critical;
        }
        else if (levelStr == "off") {
            selectedLevel = spdlog::level::off;
        }
        else {
            std::cerr << "Invalid log level: " << levelStr << std::endl;
            std::cerr << "Valid levels: trace, debug, info, warn, error, critical, off"
                      << std::endl;
            return 1;
        }
    }

    spdlog::set_level(selectedLevel);
    spdlog::info("Starting Sparkle Duck WebSocket Server");
    spdlog::debug("Log level set to: {}", args::get(logLevel));
    spdlog::info("Port: {}", port);
    if (maxSteps > 0) {
        spdlog::info("Max steps: {}", maxSteps);
    }
    else {
        spdlog::info("Running indefinitely (Ctrl+C to stop)");
    }

    // Create headless state machine.
    auto stateMachine = std::make_unique<Server::StateMachine>();
    g_stateMachine = stateMachine.get();

    // Set up signal handler for graceful shutdown.
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Create WebSocket server.
    Server::WebSocketServer server(*stateMachine, port);
    server.start();

    // Give state machine access to server for broadcasting.
    stateMachine->setWebSocketServer(&server);

    spdlog::info("WebSocket server listening on port {}", server.getPort());
    spdlog::info("Send commands to ws://localhost:{}", server.getPort());

    // Run main event loop.
    // Note: mainLoopRun() will process events until shouldExit is set.
    stateMachine->mainLoopRun();

    // Cleanup.
    server.stop();
    spdlog::info("Server shut down cleanly");

    // Print timer statistics if requested.
    if (printStats) {
        std::cout << "\n=== Server Timer Statistics ===" << std::endl;
        stateMachine->getTimers().dumpTimerStats();
    }

    return 0;
}
