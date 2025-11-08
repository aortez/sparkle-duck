#include "WebSocketServer.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

WebSocketServer::WebSocketServer(DirtSim::StateMachineInterface<Event>& stateMachine, uint16_t port)
    : stateMachine_(stateMachine)
{
    // Create WebSocket server configuration.
    rtc::WebSocketServerConfiguration config;
    config.port = port;
    config.enableTls = false; // No TLS for now.

    // Create server.
    server_ = std::make_unique<rtc::WebSocketServer>(config);

    spdlog::info("UI WebSocketServer created on port {}", port);
}

void WebSocketServer::start()
{
    // Set up client connection handler.
    server_->onClient([this](std::shared_ptr<rtc::WebSocket> ws) { onClientConnected(ws); });

    spdlog::info("UI WebSocketServer started on port {}", getPort());
}

void WebSocketServer::stop()
{
    if (server_) {
        server_->stop();
        spdlog::info("UI WebSocketServer stopped");
    }
}

uint16_t WebSocketServer::getPort() const
{
    return server_ ? server_->port() : 0;
}

void WebSocketServer::onClientConnected(std::shared_ptr<rtc::WebSocket> ws)
{
    spdlog::info("UI WebSocket client connected");

    // Set up message handler for this client.
    ws->onMessage([this, ws](std::variant<rtc::binary, rtc::string> data) {
        if (std::holds_alternative<rtc::string>(data)) {
            std::string message = std::get<rtc::string>(data);
            onMessage(ws, message);
        }
        else {
            spdlog::warn("UI WebSocket received binary message (not supported)");
        }
    });

    // Set up close handler.
    ws->onClosed([](void) { spdlog::info("UI WebSocket client disconnected"); });

    // Set up error handler.
    ws->onError([](std::string error) { spdlog::error("UI WebSocket error: {}", error); });
}

void WebSocketServer::onMessage(std::shared_ptr<rtc::WebSocket> ws, const std::string& message)
{
    spdlog::info("UI WebSocket received command: {}", message);

    // Deserialize JSON → Command.
    auto cmdResult = deserializer_.deserialize(message);
    if (cmdResult.isError()) {
        spdlog::error("UI command deserialization failed: {}", cmdResult.error().message);
        // Send error response back immediately.
        std::string errorJson = R"({"error": ")" + cmdResult.error().message + R"("})";
        ws->send(errorJson);
        return;
    }

    // Wrap Command in Cwc with response callback.
    Event cwcEvent = createCwcForCommand(cmdResult.value(), ws);

    // Queue to state machine.
    stateMachine_.queueEvent(cwcEvent);
}

Event WebSocketServer::createCwcForCommand(
    const UiApiCommand& command, std::shared_ptr<rtc::WebSocket> ws)
{
    return std::visit(
        [this, ws](auto&& cmd) -> Event {
            using CommandType = std::decay_t<decltype(cmd)>;

            // Create Cwc for each command type.
            if constexpr (std::is_same_v<CommandType, UiApi::Exit::Command>) {
                UiApi::Exit::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws](UiApi::Exit::Response&& response) {
                    std::string jsonResponse = serializer_.serialize(std::move(response));
                    ws->send(jsonResponse);
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, UiApi::SimRun::Command>) {
                UiApi::SimRun::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws](UiApi::SimRun::Response&& response) {
                    std::string jsonResponse = serializer_.serialize(std::move(response));
                    ws->send(jsonResponse);
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, UiApi::SimPause::Command>) {
                UiApi::SimPause::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws](UiApi::SimPause::Response&& response) {
                    std::string jsonResponse = serializer_.serialize(std::move(response));
                    ws->send(jsonResponse);
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, UiApi::Screenshot::Command>) {
                UiApi::Screenshot::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws](UiApi::Screenshot::Response&& response) {
                    std::string jsonResponse = serializer_.serialize(std::move(response));
                    ws->send(jsonResponse);
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, UiApi::MouseDown::Command>) {
                UiApi::MouseDown::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws](UiApi::MouseDown::Response&& response) {
                    std::string jsonResponse = serializer_.serialize(std::move(response));
                    ws->send(jsonResponse);
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, UiApi::MouseMove::Command>) {
                UiApi::MouseMove::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws](UiApi::MouseMove::Response&& response) {
                    std::string jsonResponse = serializer_.serialize(std::move(response));
                    ws->send(jsonResponse);
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, UiApi::MouseUp::Command>) {
                UiApi::MouseUp::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws](UiApi::MouseUp::Response&& response) {
                    std::string jsonResponse = serializer_.serialize(std::move(response));
                    ws->send(jsonResponse);
                };
                return cwc;
            }
            else {
                // This should never happen if UiApiCommand variant is complete.
                throw std::runtime_error("Unknown command type in createCwcForCommand");
            }
        },
        command);
}

} // namespace Ui
} // namespace DirtSim
