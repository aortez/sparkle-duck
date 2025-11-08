#pragma once

#include <string>
#include <sys/types.h>

namespace DirtSim {
namespace Client {

/**
 * @brief RAII wrapper for fork/exec/kill of server subprocess.
 */
class SubprocessManager {
public:
    SubprocessManager();
    ~SubprocessManager();

    bool launchServer(const std::string& serverPath, const std::string& args = "");
    bool waitForServerReady(const std::string& url, int timeoutSec = 5);
    void killServer();
    bool isServerRunning() const;

private:
    pid_t serverPid_ = -1;
    bool tryConnect(const std::string& url);
};

} // namespace Client
} // namespace DirtSim
