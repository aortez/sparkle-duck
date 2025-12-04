#include "WebSocketClient.h"
#include <chrono>
#include <cstring>
#include <spdlog/spdlog.h>
#include <thread>

namespace DirtSim {
namespace Network {

WebSocketClient::WebSocketClient()
{
    spdlog::debug("WebSocketClient created");
}

WebSocketClient::~WebSocketClient()
{
    disconnect();
}

Result<std::monostate, std::string> WebSocketClient::connect(const std::string& url, int timeoutMs)
{
    try {
        spdlog::debug("WebSocketClient: Connecting to {}", url);

        // Reset connection state.
        connectionFailed_ = false;
        url_ = url;

        // Create WebSocket with large message size.
        rtc::WebSocketConfiguration config;
        config.maxMessageSize = 10 * 1024 * 1024; // 10MB limit.

        ws_ = std::make_shared<rtc::WebSocket>(config);

        // Set up message handler.
        ws_->onMessage([this](std::variant<rtc::binary, rtc::string> data) {
            if (std::holds_alternative<rtc::string>(data)) {
                // JSON text message.
                std::string message = std::get<rtc::string>(data);
                spdlog::debug("WebSocketClient: Received text ({} bytes)", message.size());

                // Extract correlation ID.
                std::optional<uint64_t> correlationId;
                try {
                    nlohmann::json json = nlohmann::json::parse(message);
                    if (json.contains("id") && json["id"].is_number()) {
                        correlationId = json["id"].get<uint64_t>();
                    }
                }
                catch (...) {
                    // Not JSON or no ID - that's fine.
                }

                if (correlationId.has_value()) {
                    // Route to pending request.
                    std::lock_guard<std::mutex> lock(pendingMutex_);
                    auto it = pendingRequests_.find(*correlationId);
                    if (it != pendingRequests_.end()) {
                        auto& pending = it->second;
                        std::lock_guard<std::mutex> reqLock(pending->mutex);
                        pending->response = message;
                        pending->isBinary = false;
                        pending->received = true;
                        pending->cv.notify_one();
                    }
                }
                else if (messageCallback_) {
                    messageCallback_(message);
                }
            }
            else {
                // Binary message.
                const auto& binaryData = std::get<rtc::binary>(data);
                spdlog::debug("WebSocketClient: Received binary ({} bytes)", binaryData.size());

                // Convert to std::vector<std::byte>.
                std::vector<std::byte> bytes(binaryData.size());
                std::memcpy(bytes.data(), binaryData.data(), binaryData.size());

                // Try to extract correlation ID from envelope.
                std::optional<uint64_t> correlationId;
                try {
                    MessageEnvelope envelope = deserialize_envelope(bytes);
                    correlationId = envelope.id;
                }
                catch (...) {
                    // Not an envelope or corrupted - pass to callback.
                }

                if (correlationId.has_value()) {
                    // Route to pending request.
                    std::lock_guard<std::mutex> lock(pendingMutex_);
                    auto it = pendingRequests_.find(*correlationId);
                    if (it != pendingRequests_.end()) {
                        auto& pending = it->second;
                        std::lock_guard<std::mutex> reqLock(pending->mutex);
                        pending->response = bytes;
                        pending->isBinary = true;
                        pending->received = true;
                        pending->cv.notify_one();
                    }
                }
                else if (binaryCallback_) {
                    binaryCallback_(bytes);
                }
            }
        });

        // Set up open handler.
        ws_->onOpen([this]() {
            spdlog::debug("WebSocketClient: Connection opened");
            if (connectedCallback_) {
                connectedCallback_();
            }
        });

        // Set up close handler.
        ws_->onClosed([this]() {
            spdlog::debug("WebSocketClient: Connection closed");
            connectionFailed_ = true;
            if (disconnectedCallback_) {
                disconnectedCallback_();
            }
        });

        // Set up error handler.
        ws_->onError([this](std::string error) {
            spdlog::error("WebSocketClient error: {}", error);
            connectionFailed_ = true;
            if (errorCallback_) {
                errorCallback_(error);
            }
        });

        // Open connection.
        ws_->open(url);

        // Wait for connection with timeout.
        auto startTime = std::chrono::steady_clock::now();
        while (!ws_->isOpen() && !connectionFailed_) {
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
                > timeoutMs) {
                return Result<std::monostate, std::string>::error("Connection timeout");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (connectionFailed_) {
            return Result<std::monostate, std::string>::error("Connection failed");
        }

        spdlog::info("WebSocketClient: Connected to {}", url);
        return Result<std::monostate, std::string>::okay(std::monostate{});
    }
    catch (const std::exception& e) {
        return Result<std::monostate, std::string>::error(
            std::string("Connection error: ") + e.what());
    }
}

void WebSocketClient::disconnect()
{
    if (ws_) {
        if (ws_->isOpen()) {
            ws_->close();
        }
        ws_.reset();
    }
}

bool WebSocketClient::isConnected() const
{
    return ws_ && ws_->isOpen();
}

Result<std::monostate, std::string> WebSocketClient::sendText(const std::string& message)
{
    if (!isConnected()) {
        return Result<std::monostate, std::string>::error("Not connected");
    }

    try {
        ws_->send(message);
        return Result<std::monostate, std::string>::okay(std::monostate{});
    }
    catch (const std::exception& e) {
        return Result<std::monostate, std::string>::error(std::string("Send failed: ") + e.what());
    }
}

Result<std::monostate, std::string> WebSocketClient::sendBinary(const std::vector<std::byte>& data)
{
    if (!isConnected()) {
        return Result<std::monostate, std::string>::error("Not connected");
    }

    try {
        rtc::binary binaryMsg(data.begin(), data.end());
        ws_->send(binaryMsg);
        return Result<std::monostate, std::string>::okay(std::monostate{});
    }
    catch (const std::exception& e) {
        return Result<std::monostate, std::string>::error(std::string("Send failed: ") + e.what());
    }
}

Result<MessageEnvelope, std::string> WebSocketClient::sendBinaryAndReceive(
    const MessageEnvelope& envelope, int timeoutMs)
{
    if (!isConnected()) {
        return Result<MessageEnvelope, std::string>::error("Not connected");
    }

    uint64_t id = envelope.id;

    // Create pending request.
    auto pending = std::make_shared<PendingRequest>();
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingRequests_[id] = pending;
    }

    // Serialize and send.
    try {
        auto bytes = serialize_envelope(envelope);
        rtc::binary binaryMsg(bytes.begin(), bytes.end());

        spdlog::debug(
            "WebSocketClient: Sending binary (id={}, type={}, {} bytes)",
            id,
            envelope.message_type,
            bytes.size());

        ws_->send(binaryMsg);
    }
    catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingRequests_.erase(id);
        return Result<MessageEnvelope, std::string>::error(std::string("Send failed: ") + e.what());
    }

    // Wait for response.
    std::unique_lock<std::mutex> reqLock(pending->mutex);
    bool received = pending->cv.wait_for(
        reqLock, std::chrono::milliseconds(timeoutMs), [&pending]() { return pending->received; });

    // Clean up.
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingRequests_.erase(id);
    }

    if (!received) {
        return Result<MessageEnvelope, std::string>::error("Response timeout");
    }

    // Parse response.
    if (!pending->isBinary) {
        return Result<MessageEnvelope, std::string>::error(
            "Received text response when expecting binary");
    }

    try {
        auto& bytes = std::get<std::vector<std::byte>>(pending->response);
        MessageEnvelope responseEnvelope = deserialize_envelope(bytes);

        spdlog::debug(
            "WebSocketClient: Received binary response (id={}, type={}, {} bytes)",
            responseEnvelope.id,
            responseEnvelope.message_type,
            bytes.size());

        return Result<MessageEnvelope, std::string>::okay(responseEnvelope);
    }
    catch (const std::exception& e) {
        return Result<MessageEnvelope, std::string>::error(
            std::string("Failed to deserialize response: ") + e.what());
    }
}

Result<std::string, ApiError> WebSocketClient::sendJsonAndReceive(
    const std::string& message, int timeoutMs)
{
    if (!isConnected()) {
        return Result<std::string, ApiError>::error(ApiError{ "Not connected" });
    }

    // Generate correlation ID.
    uint64_t id = nextId_.fetch_add(1);

    // Inject ID into message.
    std::string messageWithId;
    try {
        nlohmann::json json = nlohmann::json::parse(message);
        json["id"] = id;
        messageWithId = json.dump();
    }
    catch (const std::exception& e) {
        return Result<std::string, ApiError>::error(
            ApiError{ std::string("Failed to inject correlation ID: ") + e.what() });
    }

    // Create pending request.
    auto pending = std::make_shared<PendingRequest>();
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingRequests_[id] = pending;
    }

    // Send.
    spdlog::debug("WebSocketClient: Sending JSON (id={}): {}", id, messageWithId);
    try {
        ws_->send(messageWithId);
    }
    catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingRequests_.erase(id);
        return Result<std::string, ApiError>::error(
            ApiError{ std::string("Send failed: ") + e.what() });
    }

    // Wait for response.
    std::unique_lock<std::mutex> reqLock(pending->mutex);
    bool received = pending->cv.wait_for(
        reqLock, std::chrono::milliseconds(timeoutMs), [&pending]() { return pending->received; });

    // Clean up.
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingRequests_.erase(id);
    }

    if (!received) {
        return Result<std::string, ApiError>::error(ApiError{ "Response timeout" });
    }

    if (pending->isBinary) {
        return Result<std::string, ApiError>::error(
            ApiError{ "Received binary response when expecting text" });
    }

    spdlog::debug(
        "WebSocketClient: Received JSON response (id={}, {} bytes)",
        id,
        std::get<std::string>(pending->response).size());

    return Result<std::string, ApiError>::okay(std::get<std::string>(pending->response));
}

} // namespace Network
} // namespace DirtSim
