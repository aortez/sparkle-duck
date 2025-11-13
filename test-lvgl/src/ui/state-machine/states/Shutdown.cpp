#include "State.h"
#include "ui/state-machine/StateMachine.h"
#include "ui/state-machine/network/WebSocketClient.h"
#include "ui/state-machine/network/WebSocketServer.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {
namespace State {

void Shutdown::onEnter(StateMachine& sm)
{
    spdlog::info("Shutdown: Performing cleanup");

    // Disconnect from DSSM server if connected.
    // Note: We just disconnect without sending exit command. The server is headless
    // and should keep running independently. If server shutdown is needed, it should
    // be done via separate mechanism (e.g., CLI tool).
    if (sm.wsClient_ && sm.wsClient_->isConnected()) {
        spdlog::info("Shutdown: Disconnecting from DSSM server");
        sm.wsClient_->disconnect();
    }

    // Stop WebSocket server if running.
    if (sm.wsServer_) {
        spdlog::info("Shutdown: Stopping WebSocket server");
        sm.wsServer_->stop();
    }

    // Clean up LVGL resources.
    // Note: LVGL resources are managed by unique_ptrs in StateMachine.
    // They will be automatically cleaned up when StateMachine destructor runs.
    if (sm.uiManager_) {
        spdlog::info("Shutdown: UI components will be cleaned up by StateMachine destructor");
    }

    // Set exit flag to signal main loop to exit.
    spdlog::info("Shutdown: Setting shouldExit flag to true");
    sm.setShouldExit(true);

    spdlog::info("Shutdown: Cleanup complete, shouldExit={}", sm.shouldExit());
}

} // namespace State
} // namespace Ui
} // namespace DirtSim
