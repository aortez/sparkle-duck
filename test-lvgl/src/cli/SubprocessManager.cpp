#include "SubprocessManager.h"
#include "core/network/WebSocketService.h"
#include <chrono>
#include <fcntl.h>
#include <filesystem>
#include <signal.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace DirtSim {
namespace Client {

SubprocessManager::SubprocessManager()
{}

SubprocessManager::~SubprocessManager()
{
    killUI();
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
        spdlog::debug("SubprocessManager: Launching server: {} {}", serverPath, args);

        // No need to redirect stdout - benchmark logging config disables console output.
        // Stderr is kept for crash reporting (terminate/abort messages).

        // Parse arguments into vector.
        std::vector<std::string> argVec;
        argVec.push_back(serverPath); // argv[0] is the program name.

        if (!args.empty()) {
            std::istringstream iss(args);
            std::string arg;
            while (iss >> arg) {
                argVec.push_back(arg);
            }
        }

        // Convert to char* array for execv.
        std::vector<char*> execArgs;
        for (auto& arg : argVec) {
            execArgs.push_back(const_cast<char*>(arg.c_str()));
        }
        execArgs.push_back(nullptr);

        // Execute server.
        execv(serverPath.c_str(), execArgs.data());

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

    // Check if process has exited (non-blocking).
    // Note: Using kill(pid, 0) doesn't work because zombie processes still exist.
    int status;
    pid_t result = waitpid(serverPid_, &status, WNOHANG);

    if (result == serverPid_) {
        // Process has exited (reaps zombie).
        spdlog::info("SubprocessManager: Server process {} has exited", serverPid_);
        return false;
    }
    else if (result == 0) {
        // Process still running.
        return true;
    }
    else {
        // Error or process doesn't exist.
        return false;
    }
}

bool SubprocessManager::tryConnect(const std::string& url)
{
    try {
        Network::WebSocketService client;
        auto result = client.connect(url, 1000);
        if (result.isError()) {
            return false;
        }
        client.disconnect();
        return true;
    }
    catch (const std::exception& e) {
        spdlog::debug("SubprocessManager: tryConnect exception: {}", e.what());
        return false;
    }
}

bool SubprocessManager::launchUI(const std::string& uiPath, const std::string& args)
{
    // Fork process.
    uiPid_ = fork();

    if (uiPid_ < 0) {
        spdlog::error("SubprocessManager: Failed to fork UI process");
        return false;
    }

    if (uiPid_ == 0) {
        // Child process - exec UI.
        spdlog::debug("SubprocessManager: Launching UI: {} {}", uiPath, args);

        // Parse arguments into vector.
        std::vector<std::string> argVec;
        argVec.push_back(uiPath);

        if (!args.empty()) {
            std::istringstream iss(args);
            std::string arg;
            while (iss >> arg) {
                argVec.push_back(arg);
            }
        }

        // Convert to char* array for execv.
        std::vector<char*> execArgs;
        for (auto& arg : argVec) {
            execArgs.push_back(const_cast<char*>(arg.c_str()));
        }
        execArgs.push_back(nullptr);

        // Execute UI.
        execv(uiPath.c_str(), execArgs.data());

        // If exec fails, exit child process.
        spdlog::error("SubprocessManager: execv failed for UI");
        exit(1);
    }

    // Parent process.
    spdlog::info("SubprocessManager: Launched UI (PID: {})", uiPid_);
    return true;
}

bool SubprocessManager::waitForUIReady(const std::string& url, int timeoutSec)
{
    spdlog::info("SubprocessManager: Waiting for UI to be ready at {}", url);

    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        // Check if UI process is still alive.
        if (!isUIRunning()) {
            spdlog::error("SubprocessManager: UI process died");
            return false;
        }

        // Try connecting.
        if (tryConnect(url)) {
            spdlog::info("SubprocessManager: UI is ready");
            return true;
        }

        // Check timeout.
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() >= timeoutSec) {
            spdlog::error("SubprocessManager: Timeout waiting for UI");
            return false;
        }

        // Wait a bit before retrying.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void SubprocessManager::killUI()
{
    if (uiPid_ > 0) {
        spdlog::info("SubprocessManager: Killing UI (PID: {})", uiPid_);

        // Send SIGTERM for graceful shutdown.
        kill(uiPid_, SIGTERM);

        // Wait for process to exit (with timeout).
        int status;
        int waitResult = waitpid(uiPid_, &status, WNOHANG);

        if (waitResult == 0) {
            // Process still running, wait a bit.
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            waitResult = waitpid(uiPid_, &status, WNOHANG);

            if (waitResult == 0) {
                // Still running, force kill.
                spdlog::warn("SubprocessManager: UI didn't respond to SIGTERM, sending SIGKILL");
                kill(uiPid_, SIGKILL);
                waitpid(uiPid_, &status, 0);
            }
        }

        uiPid_ = -1;
        spdlog::info("SubprocessManager: UI killed");
    }
}

bool SubprocessManager::isUIRunning() const
{
    if (uiPid_ <= 0) {
        return false;
    }

    // Check if process has exited (non-blocking).
    // Note: Using kill(pid, 0) doesn't work because zombie processes still exist.
    int status;
    pid_t result = waitpid(uiPid_, &status, WNOHANG);

    if (result == uiPid_) {
        // Process has exited (reaps zombie).
        spdlog::info("SubprocessManager: UI process {} has exited", uiPid_);
        return false;
    }
    else if (result == 0) {
        // Process still running.
        return true;
    }
    else {
        // Error or process doesn't exist.
        return false;
    }
}

} // namespace Client
} // namespace DirtSim
