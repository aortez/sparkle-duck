#include "CleanupRunner.h"
#include "core/network/WebSocketService.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <signal.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <thread>
#include <unistd.h>

namespace DirtSim {
namespace Client {

CleanupRunner::CleanupRunner()
{}

CleanupRunner::~CleanupRunner()
{}

std::vector<CleanupResult> CleanupRunner::run()
{
    std::vector<CleanupResult> results;

    // Find all sparkle-duck processes.
    auto serverPids = findProcesses("sparkle-duck-server");
    auto uiPids = findProcesses("sparkle-duck-ui");

    spdlog::info("Cleaning up sparkle-duck processes...");
    spdlog::info("Found {} server(s), {} UI(s)", serverPids.size(), uiPids.size());

    // Clean up servers (port 8080).
    for (int pid : serverPids) {
        CleanupResult result;
        result.pid = pid;
        result.processName = "sparkle-duck-server";
        result.found = true;

        auto start = std::chrono::steady_clock::now();

        spdlog::info("→ sparkle-duck-server (PID {})", pid);

        // Try WebSocket first.
        result.websocketSuccess = tryWebSocketShutdown(pid, "ws://localhost:8080", 2000);
        if (result.websocketSuccess) {
            auto end = std::chrono::steady_clock::now();
            result.shutdownTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
            spdlog::info("  ✓ Exited via WebSocket ({:.1f}ms)", result.shutdownTimeMs);
            results.push_back(result);
            continue;
        }

        // Try SIGTERM.
        spdlog::info("  ✗ WebSocket failed, trying SIGTERM");
        result.sigtermSuccess = trySigtermShutdown(pid, 2000);
        if (result.sigtermSuccess) {
            auto end = std::chrono::steady_clock::now();
            result.shutdownTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
            spdlog::info("  ✓ Exited via SIGTERM ({:.1f}ms)", result.shutdownTimeMs);
            results.push_back(result);
            continue;
        }

        // Last resort: SIGKILL.
        spdlog::info("  ✗ SIGTERM failed, trying SIGKILL");
        result.sigkillSuccess = trySigkillShutdown(pid);
        auto end = std::chrono::steady_clock::now();
        result.shutdownTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
        if (result.sigkillSuccess) {
            spdlog::info("  ✓ Killed via SIGKILL ({:.1f}ms)", result.shutdownTimeMs);
        }
        else {
            spdlog::error("  ✗ Failed to kill process!");
        }
        results.push_back(result);
    }

    // Clean up UIs (port 7070).
    for (int pid : uiPids) {
        CleanupResult result;
        result.pid = pid;
        result.processName = "sparkle-duck-ui";
        result.found = true;

        auto start = std::chrono::steady_clock::now();

        spdlog::info("→ sparkle-duck-ui (PID {})", pid);

        // Try WebSocket first.
        result.websocketSuccess = tryWebSocketShutdown(pid, "ws://localhost:7070", 2000);
        if (result.websocketSuccess) {
            auto end = std::chrono::steady_clock::now();
            result.shutdownTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
            spdlog::info("  ✓ Exited via WebSocket ({:.1f}ms)", result.shutdownTimeMs);
            results.push_back(result);
            continue;
        }

        // Try SIGTERM.
        spdlog::info("  ✗ WebSocket failed, trying SIGTERM");
        result.sigtermSuccess = trySigtermShutdown(pid, 2000);
        if (result.sigtermSuccess) {
            auto end = std::chrono::steady_clock::now();
            result.shutdownTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
            spdlog::info("  ✓ Exited via SIGTERM ({:.1f}ms)", result.shutdownTimeMs);
            results.push_back(result);
            continue;
        }

        // Last resort: SIGKILL.
        spdlog::info("  ✗ SIGTERM failed, trying SIGKILL");
        result.sigkillSuccess = trySigkillShutdown(pid);
        auto end = std::chrono::steady_clock::now();
        result.shutdownTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
        if (result.sigkillSuccess) {
            spdlog::info("  ✓ Killed via SIGKILL ({:.1f}ms)", result.shutdownTimeMs);
        }
        else {
            spdlog::error("  ✗ Failed to kill process!");
        }
        results.push_back(result);
    }

    if (results.empty()) {
        spdlog::info("No rogue processes found.");
    }
    else {
        spdlog::info("Done. Cleaned up {} process(es).", results.size());
    }

    return results;
}

std::vector<int> CleanupRunner::findProcesses(const std::string& namePattern)
{
    std::vector<int> pids;

    // Read /proc to find matching processes.
    try {
        for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
            if (!entry.is_directory()) {
                continue;
            }

            // Check if directory name is a number (PID).
            std::string dirname = entry.path().filename().string();
            if (dirname.empty() || !std::isdigit(dirname[0])) {
                continue;
            }

            int pid = std::stoi(dirname);

            // Read cmdline to get process name.
            std::ifstream cmdlineFile(entry.path() / "cmdline");
            if (!cmdlineFile) {
                continue;
            }

            std::string cmdline;
            std::getline(cmdlineFile, cmdline, '\0');

            // Check if cmdline contains our pattern.
            if (cmdline.find(namePattern) != std::string::npos) {
                // Exclude ourselves.
                if (pid != getpid()) {
                    pids.push_back(pid);
                }
            }
        }
    }
    catch (const std::exception& e) {
        spdlog::error("Error scanning /proc: {}", e.what());
    }

    return pids;
}

bool CleanupRunner::isProcessRunning(int pid)
{
    // Use kill(pid, 0) to check if process exists.
    // Returns 0 if process exists, -1 if not.
    return kill(pid, 0) == 0;
}

bool CleanupRunner::tryWebSocketShutdown(int pid, const std::string& url, int maxWaitMs)
{
    try {
        Network::WebSocketService client;

        // Try to connect (short timeout).
        auto connectResult = client.connect(url, 2000);
        if (connectResult.isError()) {
            return false;
        }

        // Send Exit command (fire-and-forget for potentially stuck processes).
        nlohmann::json exitCmd = { { "command", "Exit" } };
        auto sendResult = client.sendText(exitCmd.dump());
        if (sendResult.isError()) {
            spdlog::debug("Failed to send exit command: {}", sendResult.errorValue());
        }

        // Disconnect immediately (don't wait for response).
        client.disconnect();

        // Wait for process to exit.
        return waitForProcessExit(pid, maxWaitMs);
    }
    catch (const std::exception& e) {
        spdlog::debug("WebSocket shutdown failed: {}", e.what());
        return false;
    }
}

bool CleanupRunner::trySigtermShutdown(int pid, int maxWaitMs)
{
    if (!isProcessRunning(pid)) {
        return true; // Already dead.
    }

    // Send SIGTERM.
    if (kill(pid, SIGTERM) != 0) {
        return false;
    }

    // Wait for process to exit.
    return waitForProcessExit(pid, maxWaitMs);
}

bool CleanupRunner::trySigkillShutdown(int pid)
{
    if (!isProcessRunning(pid)) {
        return true; // Already dead.
    }

    // Send SIGKILL.
    if (kill(pid, SIGKILL) != 0) {
        return false;
    }

    // Wait a bit for kernel to clean up.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return !isProcessRunning(pid);
}

bool CleanupRunner::waitForProcessExit(int pid, int maxWaitMs)
{
    auto start = std::chrono::steady_clock::now();

    while (isProcessRunning(pid)) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

        if (elapsedMs >= maxWaitMs) {
            return false; // Timeout.
        }

        // Poll every 100ms for early exit.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return true; // Process exited!
}

} // namespace Client
} // namespace DirtSim
