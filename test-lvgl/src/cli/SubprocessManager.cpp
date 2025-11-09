#include "SubprocessManager.h"
#include "WebSocketClient.h"
#include <chrono>
#include <filesystem>
#include <signal.h>
#include <spdlog/spdlog.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace DirtSim {
namespace Client {

SubprocessManager::SubprocessManager()
{}

SubprocessManager::~SubprocessManager()
{
    killServer();
}

bool SubprocessManager::launchServer(const std::string& serverPath, const std::string& args)
{
    // Fork process.
    serverPid_ = fork();

    if (serverPid_ < 0) {
        spdlog::error("SubprocessManager: Failed to fork process");
        return false;
    }

    if (serverPid_ == 0) {
        // Child process - exec server.
        spdlog::debug("SubprocessManager: Launching server: {}", serverPath);

        // Build argument list.
        if (args.empty()) {
            execl(serverPath.c_str(), serverPath.c_str(), nullptr);
        }
        else {
            // Simple args parsing - just pass as single argument for now.
            execl(serverPath.c_str(), serverPath.c_str(), args.c_str(), nullptr);
        }

        // If exec fails, exit child process.
        spdlog::error("SubprocessManager: exec failed");
        exit(1);
    }

    // Parent process.
    spdlog::info("SubprocessManager: Launched server (PID: {})", serverPid_);
    return true;
}

bool SubprocessManager::waitForServerReady(const std::string& url, int timeoutSec)
{
    spdlog::info("SubprocessManager: Waiting for server to be ready at {}", url);

    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        // Check if server process is still alive.
        if (!isServerRunning()) {
            spdlog::error("SubprocessManager: Server process died");
            return false;
        }

        // Try connecting.
        if (tryConnect(url)) {
            spdlog::info("SubprocessManager: Server is ready");
            return true;
        }

        // Check timeout.
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() >= timeoutSec) {
            spdlog::error("SubprocessManager: Timeout waiting for server");
            return false;
        }

        // Wait a bit before retrying.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void SubprocessManager::killServer()
{
    if (serverPid_ > 0) {
        spdlog::info("SubprocessManager: Killing server (PID: {})", serverPid_);

        // Send SIGTERM for graceful shutdown.
        kill(serverPid_, SIGTERM);

        // Wait for process to exit (with timeout).
        int status;
        int waitResult = waitpid(serverPid_, &status, WNOHANG);

        if (waitResult == 0) {
            // Process still running, wait a bit.
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            waitResult = waitpid(serverPid_, &status, WNOHANG);

            if (waitResult == 0) {
                // Still running, force kill.
                spdlog::warn(
                    "SubprocessManager: Server didn't respond to SIGTERM, sending SIGKILL");
                kill(serverPid_, SIGKILL);
                waitpid(serverPid_, &status, 0);
            }
        }

        serverPid_ = -1;
        spdlog::info("SubprocessManager: Server killed");
    }
}

bool SubprocessManager::isServerRunning() const
{
    if (serverPid_ <= 0) {
        return false;
    }

    // Check if process is alive using kill with signal 0.
    int result = kill(serverPid_, 0);
    return result == 0;
}

bool SubprocessManager::tryConnect(const std::string& url)
{
    try {
        WebSocketClient client;
        if (!client.connect(url)) {
            return false;
        }
        client.disconnect();
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

} // namespace Client
} // namespace DirtSim
