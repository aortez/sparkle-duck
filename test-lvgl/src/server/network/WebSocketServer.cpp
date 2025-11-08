#include "WebSocketServer.h"
#include "server/StateMachine.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Server {

WebSocketServer::WebSocketServer(DirtSim::StateMachineInterface<Event>& stateMachine, uint16_t port)
    : stateMachine_(stateMachine)
{
    // Create WebSocket server configuration.
    rtc::WebSocketServerConfiguration config;
    config.port = port;
    config.enableTls = false; // No TLS for now.
    config.maxMessageSize = 10 * 1024 * 1024; // 10MB limit for WorldData JSON.

    // Create server.
    server_ = std::make_unique<rtc::WebSocketServer>(config);

    spdlog::info("WebSocketServer created on port {}", port);
}

void WebSocketServer::start()
{
    // Set up client connection handler.
    server_->onClient([this](std::shared_ptr<rtc::WebSocket> ws) { onClientConnected(ws); });

    spdlog::info("WebSocketServer started on port {}", getPort());
}

void WebSocketServer::stop()
{
    if (server_) {
        server_->stop();
        spdlog::info("WebSocketServer stopped");
    }
}

uint16_t WebSocketServer::getPort() const
{
    return server_ ? server_->port() : 0;
}

void WebSocketServer::onClientConnected(std::shared_ptr<rtc::WebSocket> ws)
{
    spdlog::info("WebSocket client connected");

    // Add to connected clients list.
    connectedClients_.push_back(ws);

    // Set up message handler for this client.
    ws->onMessage([this, ws](std::variant<rtc::binary, rtc::string> data) {
        if (std::holds_alternative<rtc::string>(data)) {
            std::string message = std::get<rtc::string>(data);
            onMessage(ws, message);
        }
        else {
            spdlog::warn("WebSocket received binary message (not supported)");
        }
    });

    // Set up close handler.
    ws->onClosed([this, ws](void) {
        spdlog::info("WebSocket client disconnected");
        // Remove from connected clients list.
        connectedClients_.erase(
            std::remove(connectedClients_.begin(), connectedClients_.end(), ws),
            connectedClients_.end());
    });

    // Set up error handler.
    ws->onError([](std::string error) { spdlog::error("WebSocket error: {}", error); });
}

void WebSocketServer::broadcast(const std::string& message)
{
    spdlog::trace("WebSocketServer: Broadcasting to {} clients", connectedClients_.size());

    // Send to all connected clients.
    for (auto& ws : connectedClients_) {
        if (ws && ws->isOpen()) {
            try {
                ws->send(message);
            }
            catch (const std::exception& e) {
                spdlog::error("WebSocketServer: Broadcast failed for client: {}", e.what());
            }
        }
    }
}

void WebSocketServer::onMessage(std::shared_ptr<rtc::WebSocket> ws, const std::string& message)
{
    spdlog::info("WebSocket received command: {}", message);

    // Deserialize JSON → Command.
    auto cmdResult = deserializer_.deserialize(message);
    if (cmdResult.isError()) {
        spdlog::error("Command deserialization failed: {}", cmdResult.error().message);
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
    const ApiCommand& command, std::shared_ptr<rtc::WebSocket> ws)
{
    return std::visit(
        [this, ws](auto&& cmd) -> Event {
            using CommandType = std::decay_t<decltype(cmd)>;

            // Determine the Cwc type based on command type.
            if constexpr (std::is_same_v<CommandType, Api::CellGet::Command>) {
                Api::CellGet::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws](Api::CellGet::Response&& response) {
                    std::string jsonResponse = serializer_.serialize(std::move(response));
                    ws->send(jsonResponse);
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, Api::CellSet::Command>) {
                Api::CellSet::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws](Api::CellSet::Response&& response) {
                    std::string jsonResponse = serializer_.serialize(std::move(response));
                    ws->send(jsonResponse);
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, Api::DiagramGet::Command>) {
                Api::DiagramGet::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws](Api::DiagramGet::Response&& response) {
                    std::string jsonResponse = serializer_.serialize(std::move(response));
                    spdlog::info("DiagramGet: Sending response ({} bytes)", jsonResponse.size());
                    ws->send(jsonResponse);
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, Api::GravitySet::Command>) {
                Api::GravitySet::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws](Api::GravitySet::Response&& response) {
                    std::string jsonResponse = serializer_.serialize(std::move(response));
                    ws->send(jsonResponse);
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, Api::PerfStatsGet::Command>) {
                Api::PerfStatsGet::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws](Api::PerfStatsGet::Response&& response) {
                    std::string jsonResponse = serializer_.serialize(std::move(response));
                    ws->send(jsonResponse);
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, Api::Reset::Command>) {
                Api::Reset::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws](Api::Reset::Response&& response) {
                    std::string jsonResponse = serializer_.serialize(std::move(response));
                    ws->send(jsonResponse);
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, Api::ScenarioConfigSet::Command>) {
                Api::ScenarioConfigSet::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws](Api::ScenarioConfigSet::Response&& response) {
                    std::string jsonResponse = serializer_.serialize(std::move(response));
                    ws->send(jsonResponse);
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, Api::StateGet::Command>) {
                Api::StateGet::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws](Api::StateGet::Response&& response) {
                    // Get timers for instrumentation.
                    auto& dsm = static_cast<StateMachine&>(stateMachine_);
                    auto& timers = dsm.getTimers();

                    // Time serialization (includes ReflectSerializer::to_json + nlohmann::json::dump).
                    timers.startTimer("serialize_worlddata");
                    std::string jsonResponse = serializer_.serialize(std::move(response));
                    timers.stopTimer("serialize_worlddata");

                    // Time network send.
                    timers.startTimer("network_send");
                    ws->send(jsonResponse);
                    timers.stopTimer("network_send");
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, Api::SimRun::Command>) {
                Api::SimRun::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws](Api::SimRun::Response&& response) {
                    std::string jsonResponse = serializer_.serialize(std::move(response));
                    ws->send(jsonResponse);
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, Api::StepN::Command>) {
                Api::StepN::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws](Api::StepN::Response&& response) {
                    std::string jsonResponse = serializer_.serialize(std::move(response));
                    ws->send(jsonResponse);
                };
                return cwc;
            }
            else if constexpr (std::is_same_v<CommandType, Api::Exit::Command>) {
                Api::Exit::Cwc cwc;
                cwc.command = cmd;
                cwc.callback = [this, ws](Api::Exit::Response&& response) {
                    std::string jsonResponse = serializer_.serialize(std::move(response));
                    ws->send(jsonResponse);
                };
                return cwc;
            }
            else {
                // This should never happen if ApiCommand variant is complete.
                throw std::runtime_error("Unknown command type in createCwcForCommand");
            }
        },
        command);
}

} // namespace Server
} // namespace DirtSim
