#include "StateMachine.h"
#include "core/GridOfCells.h"
#include "core/LoggingChannels.h"
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
    args::ValueFlag<std::string> logConfig(
        parser,
        "log-config",
        "Path to logging config JSON file (default: logging-config.json)",
        { "log-config" },
        "logging-config.json");
    args::ValueFlag<std::string> logChannels(
        parser,
        "channels",
        "Override log channels (e.g., swap:trace,physics:debug,*:off)",
        { 'C', "channels" });
    args::Flag printStats(
        parser, "print-stats", "Print timer statistics on exit", { "print-stats" });
    args::Flag gridCacheDisabled(
        parser,
        "no-grid-cache",
        "Disable GridOfCells bitmap cache (for benchmarking)",
        { "no-grid-cache" });

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

    // Configure GridOfCells cache (default: enabled).
    GridOfCells::USE_CACHE = !gridCacheDisabled;
    spdlog::info("GridOfCells cache: {}", GridOfCells::USE_CACHE ? "ENABLED" : "DISABLED");

    // Initialize logging from config file (supports .local override).
    std::string configPath = logConfig ? args::get(logConfig) : "logging-config.json";
    LoggingChannels::initializeFromConfig(configPath);

    // Apply command line channel overrides if provided.
    if (logChannels) {
        LoggingChannels::configureFromString(args::get(logChannels));
        spdlog::info("Applied channel overrides: {}", args::get(logChannels));
    }

    spdlog::info("Starting Sparkle Duck WebSocket Server");
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
