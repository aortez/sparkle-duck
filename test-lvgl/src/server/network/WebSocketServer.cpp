#include "WebSocketServer.h"
#include "core/MsgPackAdapter.h"
#include "server/StateMachine.h"
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

    // Deserialize JSON → Command.
    auto cmdResult = deserializer_.deserialize(message);
    if (cmdResult.isError()) {
        spdlog::error("Command deserialization failed: {}", cmdResult.error().message);
        // Send error response back immediately.
        std::string errorJson = R"({"error": ")" + cmdResult.error().message + R"("})";
        ws->send(errorJson);
        return;
    }

    // Some commands are handled immedately, by the websocket thread.
    if (std::holds_alternative<Api::StateGet::Command>(cmdResult.value())) {
        handleStateGetImmediate(ws);
        return;
    }

    // Others are queued for processing in FIFO order.
    Event cwcEvent = createCwcForCommand(cmdResult.value(), ws);
    stateMachine_.queueEvent(cwcEvent);
}

// =============================================================================
// GENERIC CWC CREATION HELPERS
// =============================================================================

namespace {

// Helper trait to map Command type to Cwc type and provide clean name.
template <typename CommandType>
struct ApiInfo;

#define REGISTER_API_NAMESPACE(NS)                          \
    template <>                                             \
    struct ApiInfo<DirtSim::Api::NS::Command> {             \
        using CommandType = DirtSim::Api::NS::Command;      \
        using CwcType = DirtSim::Api::NS::Cwc;              \
        using ResponseType = DirtSim::Api::NS::Response;    \
        static constexpr const char* name = #NS;            \
    };

// Register all API command namespaces.
// Compile-time error if a command is added to ApiCommand variant but not listed here.
REGISTER_API_NAMESPACE(CellGet)
REGISTER_API_NAMESPACE(CellSet)
REGISTER_API_NAMESPACE(DiagramGet)
REGISTER_API_NAMESPACE(Exit)
REGISTER_API_NAMESPACE(GravitySet)
REGISTER_API_NAMESPACE(PerfStatsGet)
REGISTER_API_NAMESPACE(Reset)
REGISTER_API_NAMESPACE(ScenarioConfigSet)
REGISTER_API_NAMESPACE(SeedAdd)
REGISTER_API_NAMESPACE(SimRun)
REGISTER_API_NAMESPACE(SpawnDirtBall)
REGISTER_API_NAMESPACE(StateGet)
REGISTER_API_NAMESPACE(StepN)
REGISTER_API_NAMESPACE(TimerStatsGet)  // Note: Not in deserializer yet, but needed for compile.

#undef REGISTER_API_NAMESPACE

// Primary template: creates Cwc with standard JSON serialization callback and logging.
template <typename Info>
auto makeStandardCwc(
    WebSocketServer* self,
    std::shared_ptr<rtc::WebSocket> ws,
    const typename Info::CommandType& cmd) -> typename Info::CwcType
{
    typename Info::CwcType cwc;
    cwc.command = cmd;
    cwc.callback = [self, ws](typename Info::ResponseType&& response) {
        std::string jsonResponse = self->serializer_.serialize(std::move(response));
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
    const DirtSim::Api::StateGet::Command& cmd) -> DirtSim::Api::StateGet::Cwc
{
    DirtSim::Api::StateGet::Cwc cwc;
    cwc.command = cmd;
    cwc.callback = [self, ws](DirtSim::Api::StateGet::Response&& response) {
        // Get timers for instrumentation.
        auto& dsm = static_cast<StateMachine&>(self->stateMachine_);
        auto& timers = dsm.getTimers();

        if (response.isError()) {
            // Send errors as JSON still.
            std::string jsonResponse = self->serializer_.serialize(std::move(response));
            spdlog::info("StateGet: Sending error response ({} bytes)", jsonResponse.size());
            timers.startTimer("network_send");
            ws->send(jsonResponse);
            timers.stopTimer("network_send");
        }
        else {
            // Pack WorldData directly to binary with zpp_bits.
            timers.startTimer("serialize_worlddata");

            std::vector<std::byte> data;
            zpp::bits::out out(data);
            out(response.value().worldData).or_throw();

            timers.stopTimer("serialize_worlddata");

            // Convert to rtc::binary (already std::vector<std::byte>).
            rtc::binary binaryMsg(data.begin(), data.end());

            spdlog::debug("StateGet: Sending binary response ({} bytes)", data.size());

            // Send as binary message.
            timers.startTimer("network_send");
            ws->send(binaryMsg);
            timers.stopTimer("network_send");
        }
    };
    return cwc;
}

} // anonymous namespace

Event WebSocketServer::createCwcForCommand(
    const ApiCommand& command, std::shared_ptr<rtc::WebSocket> ws)
{
    // Generic visitor that works for ALL command types.
    // Compile-time error if a command type is added to ApiCommand but not registered above.
    return std::visit(
        [this, ws](auto&& cmd) -> Event {
            using CommandType = std::decay_t<decltype(cmd)>;
            using Info = ApiInfo<CommandType>;

            return makeStandardCwc<Info>(this, ws, cmd);
        },
        command);
}

void WebSocketServer::handleStateGetImmediate(std::shared_ptr<rtc::WebSocket> ws)
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
        ws->send(errorJson);
        timers.stopTimer("state_get_immediate_total");
        return;
    }

    // Pack WorldData directly to binary with zpp_bits.
    timers.startTimer("serialize_worlddata");
    std::vector<std::byte> data;
    zpp::bits::out out(data);
    out(*cachedPtr).or_throw();
    timers.stopTimer("serialize_worlddata");

    // Convert to rtc::binary.
    rtc::binary binaryMsg(data.begin(), data.end());

    // Send immediately.
    timers.startTimer("network_send");
    ws->send(binaryMsg);
    timers.stopTimer("network_send");

    timers.stopTimer("state_get_immediate_total");
}

} // namespace Server
} // namespace DirtSim
