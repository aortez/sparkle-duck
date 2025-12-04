#pragma once

#include "BinaryProtocol.h"
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
#include <string>
#include <variant>

namespace DirtSim {
namespace Network {

/**
 * @brief Protocol format for command/response.
 */
enum class Protocol {
    BINARY, // zpp_bits serialization (fast, compact).
    JSON    // JSON serialization (human-readable, debuggable).
};

/**
 * @brief General-purpose WebSocket client with type-safe command/response handling.
 *
 * Supports both binary (zpp_bits) and JSON protocols. Binary is default for
 * performance; JSON can be used for debugging.
 *
 * Features:
 * - Result<> return types for proper error handling
 * - Type-safe sendCommand<T> template
 * - Correlation ID support for multiplexed requests
 * - Proper blocking with condition variables
 * - Async callbacks for unsolicited messages
 */
class WebSocketClient {
public:
    using MessageCallback = std::function<void(const std::string&)>;
    using BinaryCallback = std::function<void(const std::vector<std::byte>&)>;
    using ConnectionCallback = std::function<void()>;
    using ErrorCallback = std::function<void(const std::string&)>;

    WebSocketClient();
    ~WebSocketClient();

    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;

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

    // Instrumentation.
    Timers timers_;
};

// =============================================================================
// Template implementations.
// =============================================================================

template <ApiCommandType CommandT>
Result<typename CommandT::OkayType, std::string> WebSocketClient::sendCommandBinary(
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
Result<typename CommandT::OkayType, std::string> WebSocketClient::sendCommandJson(
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
