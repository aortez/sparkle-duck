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
    config.bindAddress = "0.0.0.0";           // Listen on all network interfaces.
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

    // Note: Clients must explicitly subscribe to render messages via render_format_set.
    // Not auto-subscribing saves CPU (packing/serializing) for control-only clients like
    // dashboards.

    // Set up message handler for this client.
    // Handles both JSON (text frames) and binary (zpp_bits frames).
    ws->onMessage([this, ws](std::variant<rtc::binary, rtc::string> data) {
        if (std::holds_alternative<rtc::string>(data)) {
            // Text frame = JSON protocol.
            std::string message = std::get<rtc::string>(data);
            onMessage(ws, message);
        }
        else {
            // Binary frame = zpp_bits protocol.
            const rtc::binary& binaryData = std::get<rtc::binary>(data);
            onBinaryMessage(ws, binaryData);
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
        // Send error response back immediately with correlation ID.
        nlohmann::json errorJson;
        errorJson["error"] = cmdResult.errorValue().message;
        if (correlationId.has_value()) {
            errorJson["id"] = correlationId.value();
        }
        ws->send(errorJson.dump());
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

    if (std::holds_alternative<Api::RenderFormatGet::Command>(cmdResult.value())) {
        handleRenderFormatGetImmediate(ws, correlationId);
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
REGISTER_API_NAMESPACE(PeersGet)
REGISTER_API_NAMESPACE(PerfStatsGet)
REGISTER_API_NAMESPACE(PhysicsSettingsGet)
REGISTER_API_NAMESPACE(PhysicsSettingsSet)
REGISTER_API_NAMESPACE(RenderFormatGet)
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

void WebSocketServer::handleRenderFormatGetImmediate(
    std::shared_ptr<rtc::WebSocket> ws, std::optional<uint64_t> correlationId)
{
    // Get the current render format for this client.
    RenderFormat format = getClientRenderFormat(ws);

    spdlog::info(
        "RenderFormatGet: Current format is {}", format == RenderFormat::BASIC ? "BASIC" : "DEBUG");

    // Create success response.
    Api::RenderFormatGet::Okay okay;
    okay.active_format = format;

    // Serialize to JSON.
    nlohmann::json responseJson;
    responseJson["value"] = okay.toJson();

    // Inject correlation ID if present.
    if (correlationId.has_value()) {
        responseJson["id"] = correlationId.value();
    }

    std::string jsonResponse = responseJson.dump();

    spdlog::info("RenderFormatGet: Sending response ({} bytes)", jsonResponse.size());
    ws->send(jsonResponse);
}

void WebSocketServer::broadcastRenderMessage(const World& world)
{
    const WorldData& data = world.getData();

    // Only send to clients that have explicitly subscribed via render_format_set.
    if (clientRenderFormats_.empty()) {
        return;
    }

    spdlog::trace(
        "WebSocketServer: Broadcasting RenderMessage to {} subscribed clients",
        clientRenderFormats_.size());

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

    // Pack and send to each subscribed client with their requested format.
    for (const auto& [ws, format] : clientRenderFormats_) {
        if (ws && ws->isOpen()) {
            try {

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

// =============================================================================
// BINARY PROTOCOL SUPPORT
// =============================================================================

namespace {

// Helper to deserialize a binary command payload based on message_type.
// Returns ApiCommand variant or error.
// Suppress false positive -Wmaybe-uninitialized with GCC + -O3 optimizations.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
Result<ApiCommand, ApiError> deserializeBinaryCommand(const Network::MessageEnvelope& envelope)
{
    const std::string& type = envelope.message_type;

    try {
        // Dispatch based on message_type.
        // Each branch deserializes the payload to the appropriate Command type.
        if (type == "CellGet") {
            auto cmd = Network::deserialize_payload<Api::CellGet::Command>(envelope.payload);
            return Result<ApiCommand, ApiError>::okay(cmd);
        }
        if (type == "CellSet") {
            auto cmd = Network::deserialize_payload<Api::CellSet::Command>(envelope.payload);
            return Result<ApiCommand, ApiError>::okay(cmd);
        }
        if (type == "DiagramGet") {
            auto cmd = Network::deserialize_payload<Api::DiagramGet::Command>(envelope.payload);
            return Result<ApiCommand, ApiError>::okay(cmd);
        }
        if (type == "Exit") {
            auto cmd = Network::deserialize_payload<Api::Exit::Command>(envelope.payload);
            return Result<ApiCommand, ApiError>::okay(cmd);
        }
        if (type == "GravitySet") {
            auto cmd = Network::deserialize_payload<Api::GravitySet::Command>(envelope.payload);
            return Result<ApiCommand, ApiError>::okay(cmd);
        }
        if (type == "PeersGet") {
            auto cmd = Network::deserialize_payload<Api::PeersGet::Command>(envelope.payload);
            return Result<ApiCommand, ApiError>::okay(cmd);
        }
        if (type == "PerfStatsGet") {
            auto cmd = Network::deserialize_payload<Api::PerfStatsGet::Command>(envelope.payload);
            return Result<ApiCommand, ApiError>::okay(cmd);
        }
        if (type == "PhysicsSettingsGet") {
            auto cmd =
                Network::deserialize_payload<Api::PhysicsSettingsGet::Command>(envelope.payload);
            return Result<ApiCommand, ApiError>::okay(cmd);
        }
        if (type == "PhysicsSettingsSet") {
            auto cmd =
                Network::deserialize_payload<Api::PhysicsSettingsSet::Command>(envelope.payload);
            return Result<ApiCommand, ApiError>::okay(cmd);
        }
        if (type == "RenderFormatGet") {
            auto cmd =
                Network::deserialize_payload<Api::RenderFormatGet::Command>(envelope.payload);
            return Result<ApiCommand, ApiError>::okay(cmd);
        }
        if (type == "RenderFormatSet") {
            auto cmd =
                Network::deserialize_payload<Api::RenderFormatSet::Command>(envelope.payload);
            return Result<ApiCommand, ApiError>::okay(cmd);
        }
        if (type == "Reset") {
            auto cmd = Network::deserialize_payload<Api::Reset::Command>(envelope.payload);
            return Result<ApiCommand, ApiError>::okay(cmd);
        }
        if (type == "ScenarioConfigSet") {
            auto cmd =
                Network::deserialize_payload<Api::ScenarioConfigSet::Command>(envelope.payload);
            return Result<ApiCommand, ApiError>::okay(cmd);
        }
        if (type == "SeedAdd") {
            auto cmd = Network::deserialize_payload<Api::SeedAdd::Command>(envelope.payload);
            return Result<ApiCommand, ApiError>::okay(cmd);
        }
        if (type == "SimRun") {
            auto cmd = Network::deserialize_payload<Api::SimRun::Command>(envelope.payload);
            return Result<ApiCommand, ApiError>::okay(cmd);
        }
        if (type == "SpawnDirtBall") {
            auto cmd = Network::deserialize_payload<Api::SpawnDirtBall::Command>(envelope.payload);
            return Result<ApiCommand, ApiError>::okay(cmd);
        }
        if (type == "StateGet") {
            auto cmd = Network::deserialize_payload<Api::StateGet::Command>(envelope.payload);
            return Result<ApiCommand, ApiError>::okay(cmd);
        }
        if (type == "StatusGet") {
            auto cmd = Network::deserialize_payload<Api::StatusGet::Command>(envelope.payload);
            return Result<ApiCommand, ApiError>::okay(cmd);
        }
        if (type == "TimerStatsGet") {
            auto cmd = Network::deserialize_payload<Api::TimerStatsGet::Command>(envelope.payload);
            return Result<ApiCommand, ApiError>::okay(cmd);
        }
        if (type == "WorldResize") {
            auto cmd = Network::deserialize_payload<Api::WorldResize::Command>(envelope.payload);
            return Result<ApiCommand, ApiError>::okay(cmd);
        }

        return Result<ApiCommand, ApiError>::error(
            ApiError{ "Unknown binary command type: " + type });
    }
    catch (const std::exception& e) {
        return Result<ApiCommand, ApiError>::error(
            ApiError{ std::string("Failed to deserialize binary command: ") + e.what() });
    }
}
#pragma GCC diagnostic pop

// Helper to send a binary error response.
void sendBinaryError(
    std::shared_ptr<rtc::WebSocket> ws,
    uint64_t correlationId,
    const std::string& commandName,
    const std::string& errorMessage)
{
    // Create error result.
    Result<std::monostate, ApiError> errorResult =
        Result<std::monostate, ApiError>::error(ApiError{ errorMessage });

    // Build response envelope.
    auto envelope = Network::make_response_envelope(correlationId, commandName, errorResult);

    // Serialize and send.
    auto bytes = Network::serialize_envelope(envelope);
    rtc::binary binaryMsg(bytes.begin(), bytes.end());
    ws->send(binaryMsg);
}

// Binary response callback creator - parallel to makeStandardCwc but for binary protocol.
template <typename Info>
auto makeBinaryCwc(
    [[maybe_unused]] WebSocketServer* self,
    std::shared_ptr<rtc::WebSocket> ws,
    const typename Info::CommandType& cmd,
    uint64_t correlationId) -> typename Info::CwcType
{
    typename Info::CwcType cwc;
    cwc.command = cmd;
    cwc.callback = [ws, correlationId](typename Info::ResponseType&& response) {
        // Build response envelope with serialized Result.
        auto envelope = Network::make_response_envelope(
            correlationId, std::string(Info::CommandType::name()), response);

        // Serialize envelope to bytes.
        auto bytes = Network::serialize_envelope(envelope);

        // Send as binary.
        rtc::binary binaryMsg(bytes.begin(), bytes.end());

        spdlog::info("{}: Sending binary response ({} bytes)", Info::name, bytes.size());
        ws->send(binaryMsg);
    };
    return cwc;
}

} // anonymous namespace

void WebSocketServer::onBinaryMessage(std::shared_ptr<rtc::WebSocket> ws, const rtc::binary& data)
{
    spdlog::info("WebSocket received binary command ({} bytes)", data.size());

    // Convert rtc::binary to std::vector<std::byte>.
    std::vector<std::byte> bytes(data.size());
    std::memcpy(bytes.data(), data.data(), data.size());

    // Deserialize envelope.
    Network::MessageEnvelope envelope;
    try {
        envelope = Network::deserialize_envelope(bytes);
    }
    catch (const std::exception& e) {
        spdlog::error("Failed to deserialize binary envelope: {}", e.what());
        // Can't send error response without correlation ID.
        return;
    }

    spdlog::info(
        "Binary command: type='{}', id={}, payload={} bytes",
        envelope.message_type,
        envelope.id,
        envelope.payload.size());

    // Deserialize command from envelope.
    auto cmdResult = deserializeBinaryCommand(envelope);
    if (cmdResult.isError()) {
        spdlog::error("Binary command deserialization failed: {}", cmdResult.errorValue().message);
        sendBinaryError(ws, envelope.id, envelope.message_type, cmdResult.errorValue().message);
        return;
    }

    // Handle immediate commands (same as JSON path, but with binary response).
    if (std::holds_alternative<Api::StateGet::Command>(cmdResult.value())) {
        handleStateGetImmediateBinary(ws, envelope.id);
        return;
    }

    if (std::holds_alternative<Api::StatusGet::Command>(cmdResult.value())) {
        handleStatusGetImmediateBinary(ws, envelope.id);
        return;
    }

    if (std::holds_alternative<Api::RenderFormatGet::Command>(cmdResult.value())) {
        handleRenderFormatGetImmediateBinary(ws, envelope.id);
        return;
    }

    if (std::holds_alternative<Api::RenderFormatSet::Command>(cmdResult.value())) {
        handleRenderFormatSetImmediateBinary(
            ws, std::get<Api::RenderFormatSet::Command>(cmdResult.value()), envelope.id);
        return;
    }

    // Queue other commands with binary response callback.
    Event cwcEvent = createCwcForCommandBinary(cmdResult.value(), ws, envelope.id);
    stateMachine_.queueEvent(cwcEvent);
}

Event WebSocketServer::createCwcForCommandBinary(
    const ApiCommand& command, std::shared_ptr<rtc::WebSocket> ws, uint64_t correlationId)
{
    // Generic visitor that works for ALL command types.
    return std::visit(
        [this, ws, correlationId](auto&& cmd) -> Event {
            using CommandType = std::decay_t<decltype(cmd)>;
            using Info = ApiInfo<CommandType>;

            return makeBinaryCwc<Info>(this, ws, cmd, correlationId);
        },
        command);
}

// =============================================================================
// BINARY IMMEDIATE HANDLERS
// =============================================================================

void WebSocketServer::handleStateGetImmediateBinary(
    std::shared_ptr<rtc::WebSocket> ws, uint64_t correlationId)
{
    auto& dsm = static_cast<StateMachine&>(stateMachine_);
    auto& timers = dsm.getTimers();

    timers.startTimer("state_get_immediate_binary_total");

    auto cachedPtr = dsm.getCachedWorldData();
    if (!cachedPtr) {
        spdlog::warn("WebSocketServer: state_get binary immediate - no cached data available");
        sendBinaryError(ws, correlationId, "state_get", "No world data available");
        timers.stopTimer("state_get_immediate_binary_total");
        return;
    }

    // Build success response.
    Api::StateGet::Okay okay;
    okay.worldData = *cachedPtr;
    Api::StateGet::Response response = Api::StateGet::Response::okay(std::move(okay));

    // Build response envelope.
    timers.startTimer("serialize_worlddata_binary");
    auto envelope = Network::make_response_envelope(correlationId, "state_get", response);
    auto bytes = Network::serialize_envelope(envelope);
    timers.stopTimer("serialize_worlddata_binary");

    // Send as binary.
    rtc::binary binaryMsg(bytes.begin(), bytes.end());

    spdlog::debug(
        "StateGet: Sending binary response with ID {} ({} bytes)", correlationId, bytes.size());

    timers.startTimer("network_send");
    ws->send(binaryMsg);
    timers.stopTimer("network_send");

    timers.stopTimer("state_get_immediate_binary_total");
}

void WebSocketServer::handleStatusGetImmediateBinary(
    std::shared_ptr<rtc::WebSocket> ws, uint64_t correlationId)
{
    auto& dsm = static_cast<StateMachine&>(stateMachine_);

    auto cachedPtr = dsm.getCachedWorldData();
    if (!cachedPtr) {
        spdlog::warn("WebSocketServer: status_get binary immediate - no cached data available");
        sendBinaryError(ws, correlationId, "status_get", "No world data available");
        return;
    }

    // Build lightweight status from cached data.
    Api::StatusGet::Okay okay;
    okay.timestep = cachedPtr->timestep;
    okay.scenario_id = cachedPtr->scenario_id;
    okay.width = cachedPtr->width;
    okay.height = cachedPtr->height;

    Api::StatusGet::Response response = Api::StatusGet::Response::okay(std::move(okay));

    // Build response envelope.
    auto envelope = Network::make_response_envelope(correlationId, "status_get", response);
    auto bytes = Network::serialize_envelope(envelope);

    // Send as binary.
    rtc::binary binaryMsg(bytes.begin(), bytes.end());

    spdlog::info("StatusGet: Sending binary response ({} bytes)", bytes.size());
    ws->send(binaryMsg);
}

void WebSocketServer::handleRenderFormatSetImmediateBinary(
    std::shared_ptr<rtc::WebSocket> ws,
    const Api::RenderFormatSet::Command& cmd,
    uint64_t correlationId)
{
    spdlog::info(
        "RenderFormatSet (binary): Setting format to {}",
        cmd.format == RenderFormat::BASIC ? "BASIC" : "DEBUG");

    // Set the render format for this client.
    setClientRenderFormat(ws, cmd.format);

    // Create success response.
    Api::RenderFormatSet::Okay okay;
    okay.active_format = cmd.format;
    okay.message = std::string("Render format set to ")
        + (cmd.format == RenderFormat::BASIC ? "BASIC" : "DEBUG");

    Api::RenderFormatSet::Response response = Api::RenderFormatSet::Response::okay(std::move(okay));

    // Build response envelope.
    auto envelope = Network::make_response_envelope(correlationId, "render_format_set", response);
    auto bytes = Network::serialize_envelope(envelope);

    // Send as binary.
    rtc::binary binaryMsg(bytes.begin(), bytes.end());

    spdlog::info("RenderFormatSet: Sending binary response ({} bytes)", bytes.size());
    ws->send(binaryMsg);
}

void WebSocketServer::handleRenderFormatGetImmediateBinary(
    std::shared_ptr<rtc::WebSocket> ws, uint64_t correlationId)
{
    // Get the current render format for this client.
    RenderFormat format = getClientRenderFormat(ws);

    spdlog::info(
        "RenderFormatGet (binary): Current format is {}",
        format == RenderFormat::BASIC ? "BASIC" : "DEBUG");

    // Create success response.
    Api::RenderFormatGet::Okay okay;
    okay.active_format = format;

    Api::RenderFormatGet::Response response = Api::RenderFormatGet::Response::okay(std::move(okay));

    // Build response envelope.
    auto envelope = Network::make_response_envelope(correlationId, "render_format_get", response);
    auto bytes = Network::serialize_envelope(envelope);

    // Send as binary.
    rtc::binary binaryMsg(bytes.begin(), bytes.end());

    spdlog::info("RenderFormatGet: Sending binary response ({} bytes)", bytes.size());
    ws->send(binaryMsg);
}

} // namespace Server
} // namespace DirtSim
