#include "WebSocketServer.h"

#include "server/api/ApiError.h"
#include "ui/state-machine/api/ScreenGrab.h"

#include <chrono>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

WebSocketServer::WebSocketServer(DirtSim::StateMachineInterface<Event>& stateMachine, uint16_t port)
    : stateMachine_(stateMachine)
{
    // Create WebSocket server configuration.
    rtc::WebSocketServerConfiguration config;
    config.port = port;
    config.enableTls = false;                // No TLS for now.
    config.maxMessageSize = 2 * 1024 * 1024; // 2MB max message size (for base64 screenshots).

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

// Helper to safely send response through WebSocket (catches closed socket exceptions).
static void safeSend(
    std::shared_ptr<rtc::WebSocket> ws, const std::string& message, const std::string& cmdName)
{
    try {
        ws->send(message);
        spdlog::debug("UI WebSocketServer: {} response sent ({} bytes)", cmdName, message.size());
    }
    catch (const std::exception& e) {
        spdlog::warn("UI WebSocketServer: Failed to send {} response: {}", cmdName, e.what());
    }
}

void WebSocketServer::onClientConnected(std::shared_ptr<rtc::WebSocket> ws)
{
    spdlog::info("UI WebSocket client connected");

    // Per-client state for throttling expensive operations like screen_grab.
    auto clientLastScreenshot = std::make_shared<std::chrono::steady_clock::time_point>();

    // Set up message handler for this client.
    ws->onMessage([this, ws, clientLastScreenshot](std::variant<rtc::binary, rtc::string> data) {
        if (std::holds_alternative<rtc::string>(data)) {
            std::string message = std::get<rtc::string>(data);
            onMessage(ws, message, clientLastScreenshot);
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

void WebSocketServer::onMessage(
    std::shared_ptr<rtc::WebSocket> ws,
    const std::string& message,
    std::shared_ptr<std::chrono::steady_clock::time_point> clientLastScreenshot)
{
    spdlog::info("UI WebSocket received command: {}", message);

    // Extract correlation ID from incoming message.
    std::optional<uint64_t> correlationId;
    std::string commandName;
    try {
        nlohmann::json json = nlohmann::json::parse(message);
        if (json.contains("id") && json["id"].is_number()) {
            correlationId = json["id"].get<uint64_t>();
            spdlog::debug("UI WebSocket: Correlation ID = {}", *correlationId);
        }
        if (json.contains("command") && json["command"].is_string()) {
            commandName = json["command"].get<std::string>();
        }
    }
    catch (const std::exception& e) {
        spdlog::warn("UI WebSocket: Failed to extract correlation ID: {}", e.what());
    }

    // Per-client throttle for screen_grab (before queuing to state machine).
    // This prevents dead socket requests from consuming the global throttle quota.
    if (commandName == "screen_grab" && clientLastScreenshot) {
        static constexpr std::chrono::milliseconds minInterval{ 1000 };
        auto now = std::chrono::steady_clock::now();
        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - *clientLastScreenshot);

        if (elapsed < minInterval) {
            spdlog::info(
                "UI WebSocketServer: screen_grab throttled for this client ({} ms since last, min "
                "{}ms)",
                elapsed.count(),
                minInterval.count());
            auto response = UiApi::ScreenGrab::Response::error(
                ApiError("Screenshot throttled - try again later"));
            nlohmann::json json = nlohmann::json::parse(serializer_.serialize(std::move(response)));
            if (correlationId) {
                json["id"] = *correlationId;
            }
            safeSend(ws, json.dump(), "screen_grab throttle");
            return;
        }
        // Update per-client timestamp (will be updated again on successful capture).
        *clientLastScreenshot = now;
    }

    // Deserialize JSON → Command.
    auto cmdResult = deserializer_.deserialize(message);
    if (cmdResult.isError()) {
        spdlog::error("UI command deserialization failed: {}", cmdResult.errorValue().message);
        // Send error response back immediately with correlation ID.
        nlohmann::json errorResponse;
        if (correlationId) {
            errorResponse["id"] = *correlationId;
        }
        errorResponse["success"] = false;
        errorResponse["error"] = cmdResult.errorValue().message;
        ws->send(errorResponse.dump());
        return;
    }

    // Wrap Command in Cwc with response callback (includes correlation ID).
    Event cwcEvent = createCwcForCommand(cmdResult.value(), ws, correlationId);

    // Queue to state machine.
    spdlog::info("UI WebSocketServer: Queuing event to state machine");
    stateMachine_.queueEvent(cwcEvent);
    spdlog::info("UI WebSocketServer: Event queued successfully");
}

Event WebSocketServer::createCwcForCommand(
    const UiApiCommand& command,
    std::shared_ptr<rtc::WebSocket> ws,
    std::optional<uint64_t> correlationId)
{
    return std::visit(
        [this, ws, correlationId](auto&& cmd) -> Event {
            using CommandType = std::decay_t<decltype(cmd)>;

            // Create Cwc for each command type.
            if constexpr (std::is_same_v<CommandType, UiApi::DisplayStreamStart::Command>) {
                UiApi::DisplayStreamStart::Cwc cwc;
                cwc.command = cmd;
                cwc.command.ws = ws;
                cwc.callback =
                    [this, ws, correlationId](UiApi::DisplayStreamStart::Response&& response) {
                        nlohmann::json json =
                            nlohmann::json::parse(serializer_.serialize(std::move(response)));
                        if (correlationId.has_value()) {
                            json["id"] = correlationId.value();
                        }
                        safeSend(ws, json.dump(), "Response");
                    };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, UiApi::DisplayStreamStop::Command>) {
                UiApi::DisplayStreamStop::Cwc cwc;
                cwc.command = cmd;
                cwc.command.ws = ws;
                cwc.callback =
                    [this, ws, correlationId](UiApi::DisplayStreamStop::Response&& response) {
                        nlohmann::json json =
                            nlohmann::json::parse(serializer_.serialize(std::move(response)));
                        if (correlationId.has_value()) {
                            json["id"] = correlationId.value();
                        }
                        safeSend(ws, json.dump(), "Response");
                    };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, UiApi::Exit::Command>) {
                UiApi::Exit::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws, correlationId](UiApi::Exit::Response&& response) {
                    nlohmann::json json =
                        nlohmann::json::parse(serializer_.serialize(std::move(response)));
                    if (correlationId.has_value()) {
                        json["id"] = correlationId.value();
                    }
                    safeSend(ws, json.dump(), "Response");
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, UiApi::SimRun::Command>) {
                UiApi::SimRun::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws, correlationId](UiApi::SimRun::Response&& response) {
                    nlohmann::json json =
                        nlohmann::json::parse(serializer_.serialize(std::move(response)));
                    if (correlationId.has_value()) {
                        json["id"] = correlationId.value();
                    }
                    safeSend(ws, json.dump(), "Response");
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, UiApi::SimPause::Command>) {
                UiApi::SimPause::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws, correlationId](UiApi::SimPause::Response&& response) {
                    nlohmann::json json =
                        nlohmann::json::parse(serializer_.serialize(std::move(response)));
                    if (correlationId.has_value()) {
                        json["id"] = correlationId.value();
                    }
                    safeSend(ws, json.dump(), "Response");
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, UiApi::SimStop::Command>) {
                UiApi::SimStop::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws, correlationId](UiApi::SimStop::Response&& response) {
                    nlohmann::json json =
                        nlohmann::json::parse(serializer_.serialize(std::move(response)));
                    if (correlationId.has_value()) {
                        json["id"] = correlationId.value();
                    }
                    safeSend(ws, json.dump(), "Response");
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, UiApi::ScreenGrab::Command>) {
                UiApi::ScreenGrab::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws, correlationId](UiApi::ScreenGrab::Response&& response) {
                    try {
                        nlohmann::json json =
                            nlohmann::json::parse(serializer_.serialize(std::move(response)));
                        if (correlationId.has_value()) {
                            json["id"] = correlationId.value();
                        }
                        spdlog::info(
                            "UI WebSocketServer: Sending ScreenGrab response ({} bytes)",
                            json.dump().size());
                        safeSend(ws, json.dump(), "Response");
                        spdlog::info("UI WebSocketServer: ScreenGrab response sent successfully");
                    }
                    catch (const std::exception& e) {
                        spdlog::warn(
                            "UI WebSocketServer: Failed to send ScreenGrab response: {}", e.what());
                    }
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, UiApi::MouseDown::Command>) {
                UiApi::MouseDown::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws, correlationId](UiApi::MouseDown::Response&& response) {
                    nlohmann::json json =
                        nlohmann::json::parse(serializer_.serialize(std::move(response)));
                    if (correlationId.has_value()) {
                        json["id"] = correlationId.value();
                    }
                    safeSend(ws, json.dump(), "Response");
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, UiApi::MouseMove::Command>) {
                UiApi::MouseMove::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws, correlationId](UiApi::MouseMove::Response&& response) {
                    nlohmann::json json =
                        nlohmann::json::parse(serializer_.serialize(std::move(response)));
                    if (correlationId.has_value()) {
                        json["id"] = correlationId.value();
                    }
                    safeSend(ws, json.dump(), "Response");
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, UiApi::MouseUp::Command>) {
                UiApi::MouseUp::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws, correlationId](UiApi::MouseUp::Response&& response) {
                    nlohmann::json json =
                        nlohmann::json::parse(serializer_.serialize(std::move(response)));
                    if (correlationId.has_value()) {
                        json["id"] = correlationId.value();
                    }
                    safeSend(ws, json.dump(), "Response");
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, UiApi::StatusGet::Command>) {
                UiApi::StatusGet::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws, correlationId](UiApi::StatusGet::Response&& response) {
                    nlohmann::json json =
                        nlohmann::json::parse(serializer_.serialize(std::move(response)));
                    if (correlationId.has_value()) {
                        json["id"] = correlationId.value();
                    }
                    safeSend(ws, json.dump(), "Response");
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
