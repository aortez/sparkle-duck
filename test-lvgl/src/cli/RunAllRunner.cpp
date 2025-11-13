#include "RunAllRunner.h"
#include "SubprocessManager.h"
#include "WebSocketClient.h"
#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <thread>

namespace DirtSim {
namespace Client {

Result<std::monostate, std::string> runAll(const std::string& serverPath, const std::string& uiPath)
{
    SubprocessManager subprocessManager;
    WebSocketClient client;

    // Launch server.
    std::cout << "Launching DSSM server on port 8080..." << std::endl;
    if (!subprocessManager.launchServer(serverPath, "-p 8080")) {
        return Result<std::monostate, std::string>::error("Failed to launch server");
    }

    // Wait for server to be ready.
    if (!subprocessManager.waitForServerReady("ws://localhost:8080", 10)) {
        return Result<std::monostate, std::string>::error("Server failed to start");
    }
    std::cout << "Server is ready" << std::endl;

    // Launch UI.
    std::cout << "Launching UI (Wayland backend)..." << std::endl;
    if (!subprocessManager.launchUI(uiPath, "-b wayland --connect localhost:8080")) {
        return Result<std::monostate, std::string>::error("Failed to launch UI");
    }

    // Give UI a moment to start up (no need to wait for WebSocket server).
    std::cout << "Giving UI time to start..." << std::endl;
    std::cout << "UI launched" << std::endl;
    std::cout << std::endl;
    std::cout << "=== Both server and UI are running ===" << std::endl;
    std::cout << "Server: ws://localhost:8080" << std::endl;
    std::cout << "UI:     ws://localhost:7070" << std::endl;
    std::cout << std::endl;
    std::cout << "Monitoring UI... (will shutdown server when UI exits)" << std::endl;

    // Poll UI until it exits.
    while (subprocessManager.isUIRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << std::endl;
    std::cout << "UI has exited" << std::endl;

    // Connect to server and send shutdown command.
    std::cout << "Shutting down server..." << std::endl;
    if (client.connect("ws://localhost:8080")) {
        nlohmann::json exitCmd = { { "command", "exit" } };
        client.send(exitCmd.dump());
        std::cout << "Server shutdown command sent" << std::endl;
        // Don't disconnect - let the server close the connection when it exits.
    }
    else {
        std::cout << "Server already stopped or unreachable" << std::endl;
    }

    // Wait a moment for graceful shutdown.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // SubprocessManager destructor will kill any remaining processes.
    std::cout << "Cleanup complete" << std::endl;

    return Result<std::monostate, std::string>::okay(std::monostate{});
}

} // namespace Client
} // namespace DirtSim
