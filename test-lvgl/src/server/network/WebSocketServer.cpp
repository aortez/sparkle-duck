#include "WebSocketServer.h"
#include "core/MsgPackAdapter.h"
#include "core/ReflectSerializer.h"
#include "core/RenderMessageUtils.h"
#include "core/Timers.h"
#include "core/World.h"
#include "core/organisms/TreeManager.h"
#include "server/StateMachine.h"
#include <cstring>
#include <spdlog/spdlog.h>
#include <zpp_bits.h>

namespace DirtSim {
namespace Server {

WebSocketServer::WebSocketServer(DirtSim::StateMachineInterface<Event>& stateMachine, uint16_t port)
    : stateMachine_(stateMachine)
{
    // Create WebSocket server configuration.
    rtc::WebSocketServerConfiguration config;
    config.port = port;
    config.enableTls = false;                 // No TLS for now.
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

    // Initialize render format to BASIC (default).
    clientRenderFormats_[ws] = RenderFormat::BASIC;

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
        // Remove render format tracking.
        clientRenderFormats_.erase(ws);
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

void WebSocketServer::broadcastBinary(const rtc::binary& data)
{
    spdlog::trace(
        "WebSocketServer: Broadcasting binary ({} bytes) to {} clients",
        data.size(),
        connectedClients_.size());

    // Send to all connected clients.
    for (auto& ws : connectedClients_) {
        if (ws && ws->isOpen()) {
            try {
                ws->send(data);
            }
            catch (const std::exception& e) {
                spdlog::error("WebSocketServer: Binary broadcast failed for client: {}", e.what());
            }
        }
    }
}

void WebSocketServer::onMessage(std::shared_ptr<rtc::WebSocket> ws, const std::string& message)
{
    spdlog::info("WebSocket received command: {}", message);

    // Extract correlation ID from request (optional field).
    std::optional<uint64_t> correlationId;
    try {
        nlohmann::json json = nlohmann::json::parse(message);
        if (json.contains("id") && json["id"].is_number()) {
            correlationId = json["id"].get<uint64_t>();
        }
    }
    catch (const std::exception& e) {
        spdlog::error("Failed to parse correlation ID: {}", e.what());
    }

    // Deserialize JSON → Command.
    auto cmdResult = deserializer_.deserialize(message);
    if (cmdResult.isError()) {
        spdlog::error("Command deserialization failed: {}", cmdResult.errorValue().message);
        // Send error response back immediately.
        std::string errorJson = R"({"error": ")" + cmdResult.errorValue().message + R"("})";
        ws->send(errorJson);
        return;
    }

    // Some commands are handled immediately, by the websocket thread.
    if (std::holds_alternative<Api::StateGet::Command>(cmdResult.value())) {
        handleStateGetImmediate(ws, correlationId);
        return;
    }

    if (std::holds_alternative<Api::StatusGet::Command>(cmdResult.value())) {
        handleStatusGetImmediate(ws, correlationId);
        return;
    }

    if (std::holds_alternative<Api::RenderFormatSet::Command>(cmdResult.value())) {
        handleRenderFormatSetImmediate(
            ws, std::get<Api::RenderFormatSet::Command>(cmdResult.value()), correlationId);
        return;
    }

    // Others are queued for processing in FIFO order.
    Event cwcEvent = createCwcForCommand(cmdResult.value(), ws, correlationId);
    stateMachine_.queueEvent(cwcEvent);
}

// =============================================================================
// GENERIC CWC CREATION HELPERS
// =============================================================================

namespace {

// Helper trait to map Command type to Cwc type and provide clean name.
template <typename CommandType>
struct ApiInfo;

#define REGISTER_API_NAMESPACE(NS)                       \
    template <>                                          \
    struct ApiInfo<DirtSim::Api::NS::Command> {          \
        using CommandType = DirtSim::Api::NS::Command;   \
        using CwcType = DirtSim::Api::NS::Cwc;           \
        using ResponseType = DirtSim::Api::NS::Response; \
        static constexpr const char* name = #NS;         \
    };

// Register all API command namespaces.
// Compile-time error if a command is added to ApiCommand variant but not listed here.
REGISTER_API_NAMESPACE(CellGet)
REGISTER_API_NAMESPACE(CellSet)
REGISTER_API_NAMESPACE(DiagramGet)
REGISTER_API_NAMESPACE(Exit)
REGISTER_API_NAMESPACE(GravitySet)
REGISTER_API_NAMESPACE(PerfStatsGet)
REGISTER_API_NAMESPACE(PhysicsSettingsGet)
REGISTER_API_NAMESPACE(PhysicsSettingsSet)
REGISTER_API_NAMESPACE(RenderFormatSet)
REGISTER_API_NAMESPACE(Reset)
REGISTER_API_NAMESPACE(ScenarioConfigSet)
REGISTER_API_NAMESPACE(SeedAdd)
REGISTER_API_NAMESPACE(SimRun)
REGISTER_API_NAMESPACE(SpawnDirtBall)
REGISTER_API_NAMESPACE(StateGet)
REGISTER_API_NAMESPACE(StatusGet)
REGISTER_API_NAMESPACE(TimerStatsGet)
REGISTER_API_NAMESPACE(WorldResize)

#undef REGISTER_API_NAMESPACE

// Primary template: creates Cwc with standard JSON serialization callback and logging.
template <typename Info>
auto makeStandardCwc(
    WebSocketServer* self,
    std::shared_ptr<rtc::WebSocket> ws,
    const typename Info::CommandType& cmd,
    std::optional<uint64_t> correlationId) -> typename Info::CwcType
{
    typename Info::CwcType cwc;
    cwc.command = cmd;
    cwc.callback = [self, ws, correlationId](typename Info::ResponseType&& response) {
        std::string jsonResponse = self->serializer_.serialize(std::move(response));

        // Inject correlation ID if present.
        if (correlationId.has_value()) {
            try {
                nlohmann::json json = nlohmann::json::parse(jsonResponse);
                json["id"] = correlationId.value();
                jsonResponse = json.dump();
            }
            catch (const std::exception& e) {
                spdlog::error("Failed to inject correlation ID: {}", e.what());
            }
        }

        spdlog::info("{}: Sending response ({} bytes)", Info::name, jsonResponse.size());
        ws->send(jsonResponse);
    };
    return cwc;
}

// Specialization for StateGet: binary serialization with zpp_bits.
template <>
auto makeStandardCwc<ApiInfo<DirtSim::Api::StateGet::Command>>(
    WebSocketServer* self,
    std::shared_ptr<rtc::WebSocket> ws,
    const DirtSim::Api::StateGet::Command& cmd,
    std::optional<uint64_t> correlationId) -> DirtSim::Api::StateGet::Cwc
{
    DirtSim::Api::StateGet::Cwc cwc;
    cwc.command = cmd;
    cwc.callback = [self, ws, correlationId](DirtSim::Api::StateGet::Response&& response) {
        // Get timers for instrumentation.
        auto& dsm = static_cast<StateMachine&>(self->stateMachine_);
        auto& timers = dsm.getTimers();

        if (response.isError()) {
            // Send errors as JSON.
            std::string jsonResponse = self->serializer_.serialize(std::move(response));

            // Inject correlation ID if present.
            if (correlationId.has_value()) {
                try {
                    nlohmann::json json = nlohmann::json::parse(jsonResponse);
                    json["id"] = correlationId.value();
                    jsonResponse = json.dump();
                }
                catch (const std::exception& e) {
                    spdlog::error("Failed to inject correlation ID: {}", e.what());
                }
            }

            spdlog::info("StateGet: Sending error response ({} bytes)", jsonResponse.size());
            timers.startTimer("network_send");
            ws->send(jsonResponse);
            timers.stopTimer("network_send");
        }
        else {
            // Check if this is a correlated request (needs JSON) or unsolicited push (binary).
            if (correlationId.has_value()) {
                // Explicit state_get with correlation ID - send as JSON with ID.
                timers.startTimer("serialize_worlddata");
                std::string jsonResponse = self->serializer_.serialize(std::move(response));
                timers.stopTimer("serialize_worlddata");

                // Inject correlation ID.
                try {
                    nlohmann::json json = nlohmann::json::parse(jsonResponse);
                    json["id"] = correlationId.value();
                    jsonResponse = json.dump();
                }
                catch (const std::exception& e) {
                    spdlog::error("Failed to inject correlation ID: {}", e.what());
                }

                spdlog::debug(
                    "StateGet: Sending JSON response with ID {} ({} bytes)",
                    correlationId.value(),
                    jsonResponse.size());

                timers.startTimer("network_send");
                ws->send(jsonResponse);
                timers.stopTimer("network_send");
            }
            else {
                // Unsolicited push - send as binary (no ID).
                timers.startTimer("serialize_worlddata");

                std::vector<std::byte> data;
                zpp::bits::out out(data);
                out(response.value().worldData).or_throw();

                timers.stopTimer("serialize_worlddata");

                // Convert to rtc::binary.
                rtc::binary binaryMsg(data.begin(), data.end());

                spdlog::debug("StateGet: Sending binary push ({} bytes)", data.size());

                timers.startTimer("network_send");
                ws->send(binaryMsg);
                timers.stopTimer("network_send");
            }
        }
    };
    return cwc;
}

} // anonymous namespace

Event WebSocketServer::createCwcForCommand(
    const ApiCommand& command,
    std::shared_ptr<rtc::WebSocket> ws,
    std::optional<uint64_t> correlationId)
{
    // Generic visitor that works for ALL command types.
    // Compile-time error if a command type is added to ApiCommand but not registered above.
    return std::visit(
        [this, ws, correlationId](auto&& cmd) -> Event {
            using CommandType = std::decay_t<decltype(cmd)>;
            using Info = ApiInfo<CommandType>;

            return makeStandardCwc<Info>(this, ws, cmd, correlationId);
        },
        command);
}

void WebSocketServer::handleStateGetImmediate(
    std::shared_ptr<rtc::WebSocket> ws, std::optional<uint64_t> correlationId)
{
    // Cast to concrete StateMachine type to access cached WorldData.
    auto& dsm = static_cast<StateMachine&>(stateMachine_);
    auto& timers = dsm.getTimers();

    // Track total server processing time.
    timers.startTimer("state_get_immediate_total");

    // Get cached WorldData (already updated by physics thread).
    auto cachedPtr = dsm.getCachedWorldData();
    if (!cachedPtr) {
        spdlog::warn("WebSocketServer: state_get immediate - no cached data available");
        std::string errorJson = R"({"error": "No world data available"})";

        // Inject correlation ID if present.
        if (correlationId.has_value()) {
            try {
                nlohmann::json json = nlohmann::json::parse(errorJson);
                json["id"] = correlationId.value();
                errorJson = json.dump();
            }
            catch (...) {
            }
        }

        ws->send(errorJson);
        timers.stopTimer("state_get_immediate_total");
        return;
    }

    // Check if this is a correlated request or unsolicited push.
    if (correlationId.has_value()) {
        // Explicit state_get with ID - send as JSON.
        spdlog::debug("StateGet: Handling correlated request (ID {})", correlationId.value());
        timers.startTimer("serialize_worlddata");

        try {
            // Serialize WorldData to JSON.
            nlohmann::json doc;
            doc["value"] = ReflectSerializer::to_json(*cachedPtr);
            doc["id"] = correlationId.value();
            std::string jsonResponse = doc.dump();

            timers.stopTimer("serialize_worlddata");

            spdlog::debug(
                "StateGet: Sending JSON response with ID {} ({} bytes)",
                correlationId.value(),
                jsonResponse.size());

            timers.startTimer("network_send");
            ws->send(jsonResponse);
            timers.stopTimer("network_send");
        }
        catch (const std::exception& e) {
            spdlog::error("StateGet: Failed to serialize JSON response: {}", e.what());
            timers.stopTimer("serialize_worlddata");
        }
    }
    else {
        // Unsolicited push - send as binary (more efficient).
        timers.startTimer("serialize_worlddata");

        std::vector<std::byte> data;
        zpp::bits::out out(data);
        out(*cachedPtr).or_throw();

        timers.stopTimer("serialize_worlddata");

        // Convert to rtc::binary.
        rtc::binary binaryMsg(data.begin(), data.end());

        spdlog::debug("StateGet: Sending binary push ({} bytes)", data.size());

        // Send immediately.
        timers.startTimer("network_send");
        ws->send(binaryMsg);
        timers.stopTimer("network_send");
    }

    timers.stopTimer("state_get_immediate_total");
}

void WebSocketServer::handleStatusGetImmediate(
    std::shared_ptr<rtc::WebSocket> ws, std::optional<uint64_t> correlationId)
{
    // Cast to concrete StateMachine type to access cached WorldData.
    auto& dsm = static_cast<StateMachine&>(stateMachine_);

    // Get cached WorldData (already updated by physics thread).
    auto cachedPtr = dsm.getCachedWorldData();
    if (!cachedPtr) {
        spdlog::warn("WebSocketServer: status_get immediate - no cached data available");
        std::string errorJson = R"({"error": "No world data available"})";

        // Inject correlation ID if present.
        if (correlationId.has_value()) {
            try {
                nlohmann::json json = nlohmann::json::parse(errorJson);
                json["id"] = correlationId.value();
                errorJson = json.dump();
            }
            catch (...) {
            }
        }

        ws->send(errorJson);
        return;
    }

    // Build lightweight status from cached data.
    Api::StatusGet::Okay status;
    status.timestep = cachedPtr->timestep;
    status.scenario_id = cachedPtr->scenario_id;
    status.width = cachedPtr->width;
    status.height = cachedPtr->height;

    // Serialize to JSON.
    nlohmann::json response;
    response["value"] = ReflectSerializer::to_json(status);

    // Inject correlation ID if present.
    if (correlationId.has_value()) {
        response["id"] = correlationId.value();
    }

    std::string jsonResponse = response.dump();

    spdlog::info("StatusGet: Sending response ({} bytes)", jsonResponse.size());
    ws->send(jsonResponse);
}

void WebSocketServer::handleRenderFormatSetImmediate(
    std::shared_ptr<rtc::WebSocket> ws,
    const Api::RenderFormatSet::Command& cmd,
    std::optional<uint64_t> correlationId)
{
    spdlog::info(
        "RenderFormatSet: Setting format to {}",
        cmd.format == RenderFormat::BASIC ? "BASIC" : "DEBUG");

    // Set the render format for this client.
    setClientRenderFormat(ws, cmd.format);

    // Create success response.
    Api::RenderFormatSet::Okay okay;
    okay.active_format = cmd.format;
    okay.message = std::string("Render format set to ")
        + (cmd.format == RenderFormat::BASIC ? "BASIC" : "DEBUG");

    Api::RenderFormatSet::Response response = Api::RenderFormatSet::Response::okay(std::move(okay));

    // Serialize to JSON.
    nlohmann::json responseJson;
    responseJson["value"] = okay.toJson();

    // Inject correlation ID if present.
    if (correlationId.has_value()) {
        responseJson["id"] = correlationId.value();
    }

    std::string jsonResponse = responseJson.dump();

    spdlog::info("RenderFormatSet: Sending response ({} bytes)", jsonResponse.size());
    ws->send(jsonResponse);
}

void WebSocketServer::broadcastRenderMessage(const World& world)
{
    const WorldData& data = world.getData();

    spdlog::trace(
        "WebSocketServer: Broadcasting RenderMessage to {} clients", connectedClients_.size());

    // Extract bones from all organisms.
    std::vector<BoneData> bones;
    const TreeManager& treeManager = world.getTreeManager();
    for (const auto& [tree_id, tree] : treeManager.getTrees()) {
        for (const auto& bone : tree.bones) {
            BoneData boneData;
            boneData.cell_a = bone.cell_a;
            boneData.cell_b = bone.cell_b;
            bones.push_back(boneData);
        }
    }

    // Pack and send to each client with their requested format.
    for (auto& ws : connectedClients_) {
        if (ws && ws->isOpen()) {
            try {
                // Get client's render format (defaults to BASIC).
                RenderFormat format = getClientRenderFormat(ws);

                // Pack WorldData into RenderMessage with client's format.
                RenderMessage msg = RenderMessageUtils::packRenderMessage(data, format);

                // Add extracted bones.
                msg.bones = bones;

                // Serialize to binary using zpp_bits.
                std::vector<std::byte> msgData;
                zpp::bits::out out(msgData);
                out(msg).or_throw();

                // Send binary message.
                rtc::binary binaryMsg(msgData.begin(), msgData.end());
                ws->send(binaryMsg);

                spdlog::trace(
                    "WebSocketServer: Sent RenderMessage ({} bytes, format={}) to client",
                    msgData.size(),
                    static_cast<int>(format));
            }
            catch (const std::exception& e) {
                spdlog::error(
                    "WebSocketServer: RenderMessage broadcast failed for client: {}", e.what());
            }
        }
    }
}

void WebSocketServer::setClientRenderFormat(std::shared_ptr<rtc::WebSocket> ws, RenderFormat format)
{
    clientRenderFormats_[ws] = format;
    spdlog::info(
        "WebSocketServer: Client render format set to {}",
        format == RenderFormat::BASIC ? "BASIC" : "DEBUG");
}

RenderFormat WebSocketServer::getClientRenderFormat(std::shared_ptr<rtc::WebSocket> ws) const
{
    auto it = clientRenderFormats_.find(ws);
    if (it != clientRenderFormats_.end()) {
        return it->second;
    }
    return RenderFormat::BASIC; // Default.
}

} // namespace Server
} // namespace DirtSim
