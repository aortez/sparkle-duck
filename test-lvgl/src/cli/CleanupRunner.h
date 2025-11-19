#pragma once

#include <string>
#include <vector>

namespace DirtSim {
namespace Client {

/**
 * @brief Result of cleaning up a single process.
 */
struct CleanupResult {
    int pid;
    std::string processName;
    bool found;
    bool websocketSuccess;
    bool sigtermSuccess;
    bool sigkillSuccess;
    double shutdownTimeMs;
};

/**
 * @brief Finds and gracefully shuts down rogue sparkle-duck processes.
 *
 * Shutdown cascade:
 * 1. Try WebSocket API (Exit command) - most graceful
 * 2. Try SIGTERM - graceful OS signal
 * 3. Try SIGKILL - force kill (last resort)
 *
 * All waits exit early if process dies before timeout.
 */
class CleanupRunner {
public:
    CleanupRunner();
    ~CleanupRunner();

    /**
     * @brief Find and clean up all sparkle-duck processes.
     * @return Vector of cleanup results (one per process found).
     */
    std::vector<CleanupResult> run();

private:
    /**
     * @brief Find PIDs of processes matching name pattern.
     */
    std::vector<int> findProcesses(const std::string& namePattern);

    /**
     * @brief Check if a process is running.
     */
    bool isProcessRunning(int pid);

    /**
     * @brief Try to shutdown via WebSocket Exit command.
     * @return true if process exited.
     */
    bool tryWebSocketShutdown(int pid, const std::string& url, int maxWaitMs);

    /**
     * @brief Try to shutdown via SIGTERM.
     * @return true if process exited.
     */
    bool trySigtermShutdown(int pid, int maxWaitMs);

    /**
     * @brief Force kill via SIGKILL.
     * @return true if process was killed.
     */
    bool trySigkillShutdown(int pid);

    /**
     * @brief Wait for process to exit, polling every 100ms.
     * @return true if process exited within timeout.
     */
    bool waitForProcessExit(int pid, int maxWaitMs);
};

} // namespace Client
} // namespace DirtSim
