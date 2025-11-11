#pragma once

#include <string>
#include <sys/types.h>

namespace DirtSim {
namespace Client {

/**
 * @brief RAII wrapper for fork/exec/kill of server and UI subprocesses.
 */
class SubprocessManager {
public:
    SubprocessManager();
    ~SubprocessManager();

    bool launchServer(const std::string& serverPath, const std::string& args = "");
    bool launchUI(const std::string& uiPath, const std::string& args = "");
    bool waitForServerReady(const std::string& url, int timeoutSec = 5);
    bool waitForUIReady(const std::string& url, int timeoutSec = 5);
    void killServer();
    void killUI();
    bool isServerRunning() const;
    bool isUIRunning() const;

private:
    pid_t serverPid_ = -1;
    pid_t uiPid_ = -1;
    bool tryConnect(const std::string& url);
};

} // namespace Client
} // namespace DirtSim
