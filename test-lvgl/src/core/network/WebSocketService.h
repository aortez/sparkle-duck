#pragma once

#include "BinaryProtocol.h"
#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include "core/Timers.h"
#include "server/api/ApiCommand.h"
#include "server/api/ApiError.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <rtc/rtc.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <variant>

namespace DirtSim {

class World; // Forward declaration.

namespace Network {

/**
 * @brief Protocol format for command/response.
 */
enum class Protocol {
    BINARY, // zpp_bits serialization (fast, compact).
    JSON    // JSON serialization (human-readable, debuggable).
};

/**
 * @brief Unified WebSocket service supporting both client and server roles.
 *
 * Can simultaneously act as:
 * - Client: Connect to remote endpoints, send commands, receive responses
 * - Server: Listen for connections, handle incoming commands via registered handlers
 *
 * Supports binary (zpp_bits) protocol by default. JSON available for debugging/CLI.
 *
 * Features:
 * - Result<> return types for proper error handling
 * - Type-safe command templates with automatic name derivation
 * - Correlation ID support for multiplexed requests
 * - Template-based handler registration (server side)
 * - Async callbacks for unsolicited messages
 */
class WebSocketService {
public:
    using MessageCallback = std::function<void(const std::string&)>;
    using BinaryCallback = std::function<void(const std::vector<std::byte>&)>;
    using ConnectionCallback = std::function<void()>;
    using ErrorCallback = std::function<void(const std::string&)>;

    WebSocketService();
    ~WebSocketService();

    WebSocketService(const WebSocketService&) = delete;
    WebSocketService& operator=(const WebSocketService&) = delete;

    Result<std::monostate, std::string> connect(const std::string& url, int timeoutMs = 5000);

    void disconnect();

    bool isConnected() const;

    std::string getUrl() const { return url_; }

    void setProtocol(Protocol protocol) { protocol_ = protocol; }

    Protocol getProtocol() const { return protocol_; }

    template <ApiCommandType CommandT>
    Result<typename CommandT::OkayType, std::string> sendCommand(
        const CommandT& cmd, int timeoutMs = 5000)
    {
        if (protocol_ == Protocol::BINARY) {
            return sendCommandBinary<CommandT>(cmd, timeoutMs);
        }
        else {
            return sendCommandJson<CommandT>(cmd, timeoutMs);
        }
    }

    // =========================================================================
    // Raw send (for advanced use cases and dynamic dispatch).
    // =========================================================================

    /**
     * @brief Send raw text message (fire-and-forget).
     */
    Result<std::monostate, std::string> sendText(const std::string& message);

    /**
     * @brief Send raw binary message (fire-and-forget).
     */
    Result<std::monostate, std::string> sendBinary(const std::vector<std::byte>& data);

    /**
     * @brief Send JSON and receive response (for dynamic dispatch).
     *
     * Useful when command type isn't known at compile time (e.g., CLI parsing strings).
     * For type-safe usage, prefer sendCommand<T>().
     *
     * @param message JSON message to send.
     * @param timeoutMs Timeout in milliseconds.
     * @return Result with JSON response string on success, ApiError on failure.
     */
    Result<std::string, ApiError> sendJsonAndReceive(
        const std::string& message, int timeoutMs = 5000);

    /**
     * @brief Send binary envelope and receive response (for manual testing).
     *
     * Useful for testing binary protocol or when you need full control.
     *
     * @param envelope The message envelope to send.
     * @param timeoutMs Timeout in milliseconds.
     * @return Result with response envelope on success, error on failure.
     */
    Result<MessageEnvelope, std::string> sendBinaryAndReceive(
        const MessageEnvelope& envelope, int timeoutMs = 5000);

    // =========================================================================
    // Callbacks for async/unsolicited messages.
    // =========================================================================

    void onMessage(MessageCallback callback) { messageCallback_ = callback; }
    void onBinary(BinaryCallback callback) { binaryCallback_ = callback; }
    void onConnected(ConnectionCallback callback) { connectedCallback_ = callback; }
    void onDisconnected(ConnectionCallback callback) { disconnectedCallback_ = callback; }
    void onError(ErrorCallback callback) { errorCallback_ = callback; }

    // =========================================================================
    // Server-side methods (listening for connections).
    // =========================================================================

    /**
     * @brief Start listening for incoming WebSocket connections.
     *
     * @param port Port to listen on.
     * @return Result with success or error message.
     */
    Result<std::monostate, std::string> listen(uint16_t port);

    /**
     * @brief Stop listening for connections.
     */
    void stopListening();

    /**
     * @brief Check if server is currently listening.
     */
    bool isListening() const;

    /**
     * @brief Broadcast binary message to all connected clients.
     * @param data Binary data to send.
     */
    void broadcastBinary(const std::vector<std::byte>& data);

    /**
     * @brief Register a typed command handler (server-side).
     *
     * Handler receives CommandWithCallback and calls sendResponse() when done.
     * Supports both immediate (synchronous) and queued (asynchronous) handlers.
     *
     * @tparam CwcT The CommandWithCallback type (e.g., Api::StateGet::Cwc).
     * @param handler Function that receives CWC and eventually calls sendResponse().
     *
     * Example:
     *   service.registerHandler<Api::StateGet::Cwc>([](Api::StateGet::Cwc cwc) {
     *       // Immediate response
     *       cwc.sendResponse(Api::StateGet::Response::okay(getState()));
     *   });
     *
     *   service.registerHandler<Api::SimRun::Cwc>([sm](Api::SimRun::Cwc cwc) {
     *       // Queue to state machine, respond later
     *       sm->queueEvent(cwc);  // State machine calls sendResponse() when done
     *   });
     */
    template <typename CwcT>
    void registerHandler(std::function<void(CwcT)> handler)
    {
        using CommandT = typename CwcT::Command;
        using ResponseT = typename CwcT::Response;

        std::string cmdName(CommandT::name());
        spdlog::debug("WebSocketService: Registering handler for '{}'", cmdName);

        // Wrap typed handler in generic handler that handles serialization.
        commandHandlers_[cmdName] = [handler, cmdName](
                                        const std::vector<std::byte>& payload,
                                        std::shared_ptr<rtc::WebSocket> ws,
                                        uint64_t correlationId) {
            // Deserialize payload → typed command.
            CommandT cmd;
            try {
                cmd = Network::deserialize_payload<CommandT>(payload);
            }
            catch (const std::exception& e) {
                spdlog::error("Failed to deserialize {}: {}", cmdName, e.what());
                // TODO: Send error response.
                return;
            }

            // Build CWC with callback that sends response.
            CwcT cwc;
            cwc.command = cmd;
            cwc.callback = [ws, correlationId, cmdName](ResponseT&& response) {
                // Serialize response → envelope → binary.
                auto envelope =
                    Network::make_response_envelope(correlationId, std::string(cmdName), response);
                auto bytes = Network::serialize_envelope(envelope);

                // Send binary response.
                rtc::binary binaryMsg(bytes.begin(), bytes.end());
                spdlog::debug(
                    "WebSocketService: Sending {} response ({} bytes)", cmdName, bytes.size());
                ws->send(binaryMsg);
            };

            // Call handler - it will call cwc.sendResponse() or cwc.callback() when ready.
            handler(std::move(cwc));
        };
    }

    // =========================================================================
    // Instrumentation.
    // =========================================================================

    Timers& getTimers() { return timers_; }

private:
    // Protocol-specific command implementations.
    template <ApiCommandType CommandT>
    Result<typename CommandT::OkayType, std::string> sendCommandBinary(
        const CommandT& cmd, int timeoutMs);

    template <ApiCommandType CommandT>
    Result<typename CommandT::OkayType, std::string> sendCommandJson(
        const CommandT& cmd, int timeoutMs);

    // WebSocket connection.
    std::shared_ptr<rtc::WebSocket> ws_;
    std::string url_;
    Protocol protocol_ = Protocol::BINARY;

    // Connection state.
    std::atomic<bool> connectionFailed_{ false };

    // Pending requests with correlation IDs.
    struct PendingRequest {
        std::variant<std::string, std::vector<std::byte>> response;
        bool received = false;
        bool isBinary = false;
        std::mutex mutex;
        std::condition_variable cv;
    };
    std::atomic<uint64_t> nextId_{ 1 };
    std::map<uint64_t, std::shared_ptr<PendingRequest>> pendingRequests_;
    std::mutex pendingMutex_;

    // Callbacks.
    MessageCallback messageCallback_;
    BinaryCallback binaryCallback_;
    ConnectionCallback connectedCallback_;
    ConnectionCallback disconnectedCallback_;
    ErrorCallback errorCallback_;

    // =========================================================================
    // Server-side state (listening for connections).
    // =========================================================================

    using CommandHandler = std::function<void(
        const std::vector<std::byte>& payload,
        std::shared_ptr<rtc::WebSocket> ws,
        uint64_t correlationId)>;

    std::unique_ptr<rtc::WebSocketServer> server_;
    std::map<std::string, CommandHandler> commandHandlers_;
    std::vector<std::shared_ptr<rtc::WebSocket>> connectedClients_;

    void onClientConnected(std::shared_ptr<rtc::WebSocket> ws);
    void onClientMessage(std::shared_ptr<rtc::WebSocket> ws, const rtc::binary& data);

    // Instrumentation.
    Timers timers_;
};

// =============================================================================
// Template implementations.
// =============================================================================

template <ApiCommandType CommandT>
Result<typename CommandT::OkayType, std::string> WebSocketService::sendCommandBinary(
    const CommandT& cmd, int timeoutMs)
{
    // Build command envelope.
    uint64_t id = nextId_.fetch_add(1);
    auto envelope = make_command_envelope(id, cmd);

    // Send and receive.
    auto result = sendBinaryAndReceive(envelope, timeoutMs);
    if (result.isError()) {
        return Result<typename CommandT::OkayType, std::string>::error(result.errorValue());
    }

    const MessageEnvelope& responseEnvelope = result.value();

    // Verify response type.
    std::string expectedType = std::string(CommandT::name()) + "_response";
    if (responseEnvelope.message_type != expectedType) {
        return Result<typename CommandT::OkayType, std::string>::error(
            "Unexpected response type: " + responseEnvelope.message_type + " (expected "
            + expectedType + ")");
    }

    // Extract result from envelope.
    try {
        auto extractedResult =
            extract_result<typename CommandT::OkayType, ApiError>(responseEnvelope);
        if (extractedResult.isError()) {
            return Result<typename CommandT::OkayType, std::string>::error(
                extractedResult.errorValue().message);
        }
        return Result<typename CommandT::OkayType, std::string>::okay(extractedResult.value());
    }
    catch (const std::exception& e) {
        return Result<typename CommandT::OkayType, std::string>::error(
            std::string("Failed to extract result: ") + e.what());
    }
}

template <ApiCommandType CommandT>
Result<typename CommandT::OkayType, std::string> WebSocketService::sendCommandJson(
    const CommandT& cmd, int timeoutMs)
{
    // Build JSON message.
    nlohmann::json json = cmd.toJson();
    json["command"] = std::string(CommandT::name());

    // Send and receive.
    auto result = sendJsonAndReceive(json.dump(), timeoutMs);
    if (result.isError()) {
        return Result<typename CommandT::OkayType, std::string>::error(result.errorValue().message);
    }

    // Parse response.
    nlohmann::json responseJson;
    try {
        responseJson = nlohmann::json::parse(result.value());
    }
    catch (const std::exception& e) {
        return Result<typename CommandT::OkayType, std::string>::error(
            std::string("Invalid JSON response: ") + e.what());
    }

    // Check for error.
    if (responseJson.contains("error")) {
        std::string errorMsg = "Unknown error";
        if (responseJson["error"].is_string()) {
            errorMsg = responseJson["error"].get<std::string>();
        }
        else if (responseJson["error"].is_object() && responseJson["error"].contains("message")) {
            errorMsg = responseJson["error"]["message"].get<std::string>();
        }
        return Result<typename CommandT::OkayType, std::string>::error(errorMsg);
    }

    // Extract value.
    if (!responseJson.contains("value")) {
        return Result<typename CommandT::OkayType, std::string>::error(
            "Response missing 'value' field");
    }

    try {
        if constexpr (std::is_same_v<typename CommandT::OkayType, std::monostate>) {
            return Result<typename CommandT::OkayType, std::string>::okay(std::monostate{});
        }
        else {
            auto okay = CommandT::OkayType::fromJson(responseJson["value"]);
            return Result<typename CommandT::OkayType, std::string>::okay(std::move(okay));
        }
    }
    catch (const std::exception& e) {
        return Result<typename CommandT::OkayType, std::string>::error(
            std::string("Failed to deserialize response: ") + e.what());
    }
}

} // namespace Network
} // namespace DirtSim
