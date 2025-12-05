#include "RunAllRunner.h"
#include "SubprocessManager.h"
#include "core/network/WebSocketService.h"
#include "server/api/Exit.h"
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
    Network::WebSocketService client;

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

    // Auto-detect display backend.
    std::string backend = "x11"; // Default to X11 for better compatibility
    const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY");
    const char* x11Display = std::getenv("DISPLAY");

    if (waylandDisplay && waylandDisplay[0] != '\0') {
        backend = "wayland";
        std::cout << "Detected Wayland display, using Wayland backend" << std::endl;
    }
    else if (x11Display && x11Display[0] != '\0') {
        backend = "x11";
        std::cout << "Detected X11 display, using X11 backend" << std::endl;
    }
    else {
        std::cout << "Warning: No display detected, attempting X11 backend" << std::endl;
    }

    // Launch UI.
    std::cout << "Launching UI (" << backend << " backend)..." << std::endl;
    std::string uiArgs = "-b " + backend + " --connect localhost:8080";
    if (!subprocessManager.launchUI(uiPath, uiArgs)) {
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
    auto connectResult = client.connect("ws://localhost:8080");
    if (connectResult.isValue()) {
        const DirtSim::Api::Exit::Command cmd{};
        nlohmann::json exitCmd = cmd.toJson();
        exitCmd["command"] = DirtSim::Api::Exit::Command::name();
        auto exitResult = client.sendJsonAndReceive(exitCmd.dump(), 2000);
        if (exitResult.isValue()) {
            std::cout << "Server acknowledged shutdown" << std::endl;
        }
        client.disconnect();
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
