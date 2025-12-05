#include "IntegrationTest.h"
#include "SubprocessManager.h"
#include "core/network/BinaryProtocol.h"
#include "core/network/WebSocketService.h"
#include "server/api/Exit.h"
#include "server/api/SimRun.h"
#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <thread>

namespace DirtSim {
namespace Client {

IntegrationTest::IntegrationTest()
{}

IntegrationTest::~IntegrationTest()
{}

int IntegrationTest::run(const std::string& serverPath, const std::string& uiPath)
{
    SubprocessManager subprocessManager;
    Network::WebSocketService client;

    // Launch server.
    std::cout << "Launching server..." << std::endl;
    if (!subprocessManager.launchServer(serverPath, "-p 8080")) {
        std::cerr << "Error: Failed to launch server" << std::endl;
        return 1;
    }

    // Wait for server to be ready.
    if (!subprocessManager.waitForServerReady("ws://localhost:8080", 10)) {
        std::cerr << "Error: Server failed to start" << std::endl;
        return 1;
    }
    std::cout << "Server is ready" << std::endl;

    // Launch UI.
    std::cout << "Launching UI..." << std::endl;
    if (!subprocessManager.launchUI(uiPath, "-b wayland --connect localhost:8080")) {
        std::cerr << "Error: Failed to launch UI" << std::endl;
        return 1;
    }

    // Wait for UI to be ready (UI runs WebSocket server on port 7070).
    if (!subprocessManager.waitForUIReady("ws://localhost:7070", 10)) {
        std::cerr << "Error: UI failed to start" << std::endl;
        return 1;
    }
    std::cout << "UI is ready" << std::endl;

    // Connect to server.
    auto connectResult = client.connect("ws://localhost:8080");
    if (connectResult.isError()) {
        std::cerr << "Error: Failed to connect to server: " << connectResult.errorValue()
                  << std::endl;
        return 1;
    }

    // Start simulation (creates World and transitions to SimRunning).
    std::cout << "Starting simulation..." << std::endl;
    const DirtSim::Api::SimRun::Command simCmd{ .timestep = 0.016, .max_steps = 1 };
    auto simEnvelope = DirtSim::Network::make_command_envelope(1, simCmd);
    auto simResult = client.sendBinaryAndReceive(simEnvelope, 5000);
    if (simResult.isError()) {
        std::cerr << "Error: Failed to start simulation: " << simResult.errorValue() << std::endl;
        return 1;
    }
    std::cout << "Simulation started" << std::endl;

    // Wait for simulation to complete the single step.
    std::cout << "Waiting for simulation to complete..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Send exit to server and wait for acknowledgment.
    std::cout << "Shutting down server..." << std::endl;
    const DirtSim::Api::Exit::Command exitCmd{};
    auto exitEnvelope = DirtSim::Network::make_command_envelope(2, exitCmd);
    auto exitResult = client.sendBinaryAndReceive(exitEnvelope, 2000);
    if (exitResult.isValue()) {
        std::cout << "Server acknowledged shutdown" << std::endl;
    }
    client.disconnect();

    // Wait for server to exit gracefully.
    for (int i = 0; i < 10; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!subprocessManager.isServerRunning()) {
            std::cout << "Server exited cleanly" << std::endl;
            break;
        }
    }

    // Kill UI (SIGTERM).
    std::cout << "Shutting down UI..." << std::endl;
    subprocessManager.killUI();

    // Wait for processes to exit cleanly.
    for (int i = 0; i < 10; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!subprocessManager.isServerRunning() && !subprocessManager.isUIRunning()) {
            std::cout << "Server and UI exited cleanly" << std::endl;
            break;
        }
    }

    // Verify both are stopped (forcefully kill if needed).
    if (subprocessManager.isServerRunning()) {
        std::cout << "Server didn't exit gracefully, force killing..." << std::endl;
        subprocessManager.killServer();
    }

    if (subprocessManager.isUIRunning()) {
        std::cout << "UI didn't exit gracefully, force killing..." << std::endl;
        subprocessManager.killUI();
    }

    std::cout << "Integration test PASSED" << std::endl;
    std::cout << "- Server launched and connected successfully" << std::endl;
    std::cout << "- UI launched and connected successfully" << std::endl;
    std::cout << "- Simulation started successfully" << std::endl;
    std::cout << "- Both processes cleaned up" << std::endl;
    return 0;
}

} // namespace Client
} // namespace DirtSim
