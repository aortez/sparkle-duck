#include "../StateMachine.h"
#include "../network/WebSocketClient.h"
#include "State.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {
namespace State {

void Disconnected::onEnter(StateMachine& /*sm*/)
{
    spdlog::info("Disconnected: Not connected to DSSM server");
    spdlog::info("Disconnected: Show connection UI (host/port input, connect button)");
    // TODO: Display connection UI using SimulatorUI.
}

void Disconnected::onExit(StateMachine& /*sm*/)
{
    spdlog::info("Disconnected: Exiting");
}

State::Any Disconnected::onEvent(const ConnectToServerCommand& cmd, StateMachine& sm)
{
    spdlog::info("Disconnected: Connect command received (host={}, port={})", cmd.host, cmd.port);

    // Get WebSocket client from state machine.
    auto* wsClient = sm.getWebSocketClient();
    if (!wsClient) {
        spdlog::error("Disconnected: No WebSocket client available");
        return Disconnected{};
    }

    // Set up callbacks before connecting.
    wsClient->onConnected([&sm]() {
        spdlog::info("Disconnected: DSSM connection established");
        // Queue ServerConnectedEvent to trigger state transition.
        sm.queueEvent(ServerConnectedEvent{});
    });

    wsClient->onDisconnected([&sm]() {
        spdlog::warn("Disconnected: DSSM connection lost");
        sm.queueEvent(ServerDisconnectedEvent{"Connection closed"});
    });

    wsClient->onError([&sm](const std::string& error) {
        spdlog::error("Disconnected: DSSM connection error: {}", error);
        sm.queueEvent(ServerDisconnectedEvent{error});
    });

    wsClient->onMessage([&sm](const std::string& message) {
        spdlog::debug("UI: Received message from DSSM: {}", message);

        // Parse message to determine type.
        try {
            nlohmann::json msg = nlohmann::json::parse(message);

            // Check if it's a frame_ready notification.
            if (msg.contains("type") && msg["type"] == "frame_ready") {
                uint64_t stepNumber = msg.value("stepNumber", 0);
                int64_t timestamp = msg.value("timestamp", 0);
                spdlog::info("UI: Received frame_ready notification (step {})", stepNumber);

                // Queue FrameReadyNotification event.
                sm.queueEvent(FrameReadyNotification{stepNumber, timestamp});
            }
            else {
                // Regular response (success/error).
                spdlog::debug("UI: Received response from DSSM: {}", message);
            }
        }
        catch (const std::exception& e) {
            spdlog::error("UI: Failed to parse DSSM message: {}", e.what());
        }
    });

    // Initiate connection.
    std::string url = "ws://" + cmd.host + ":" + std::to_string(cmd.port);
    bool success = wsClient->connect(url);

    if (!success) {
        spdlog::error("Disconnected: Failed to initiate connection to {}", url);
    }

    // Stay in Disconnected state - will transition to StartMenu on ServerConnectedEvent.
    return Disconnected{};
}

State::Any Disconnected::onEvent(const ServerConnectedEvent& /*evt*/, StateMachine& /*sm*/)
{
    spdlog::info("Disconnected: Server connection established");
    spdlog::info("Disconnected: Transitioning to StartMenu");

    // Transition to StartMenu state (show simulation controls).
    return StartMenu{};
}

State::Any Disconnected::onEvent(const UiApi::Exit::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::info("Disconnected: Exit command received, shutting down");

    // Send success response.
    cwc.sendResponse(UiApi::Exit::Response::okay(std::monostate{}));

    // Transition to Shutdown state.
    return Shutdown{};
}

} // namespace State
} // namespace Ui
} // namespace DirtSim
