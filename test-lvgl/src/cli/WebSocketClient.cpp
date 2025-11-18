#include "WebSocketClient.h"
#include "core/MsgPackAdapter.h"
#include "core/ReflectSerializer.h"
#include "core/WorldData.h"
#include <chrono>
#include <spdlog/spdlog.h>
#include <thread>
#include <zpp_bits.h>

namespace DirtSim {
namespace Client {

WebSocketClient::WebSocketClient() : responseReceived_(false), connectionFailed_(false)
{}

WebSocketClient::~WebSocketClient()
{
    disconnect();
}

bool WebSocketClient::connect(const std::string& url)
{
    try {
        spdlog::debug("WebSocketClient: Connecting to {}", url);

        // Reset connection state.
        connectionFailed_ = false;

        // Create WebSocket with large message size for WorldData.
        rtc::WebSocketConfiguration config;
        config.maxMessageSize = 10 * 1024 * 1024; // 10MB limit for WorldData JSON.

        ws_ = std::make_shared<rtc::WebSocket>(config);

        // Set up message handler (supports both blocking and async modes, JSON and CBOR).
        ws_->onMessage([this](std::variant<rtc::binary, rtc::string> data) {
            std::string message;
            std::optional<uint64_t> correlationId;

            if (std::holds_alternative<rtc::string>(data)) {
                // JSON string message.
                message = std::get<rtc::string>(data);
                spdlog::debug("WebSocketClient: Received JSON response ({} bytes)", message.size());

                // Check for correlation ID.
                try {
                    nlohmann::json json = nlohmann::json::parse(message);
                    if (json.contains("id") && json["id"].is_number()) {
                        correlationId = json["id"].get<uint64_t>();
                        spdlog::debug(
                            "WebSocketClient: Message has correlation ID: {}", *correlationId);
                    }
                }
                catch (const std::exception& e) {
                    spdlog::debug("WebSocketClient: Failed to parse correlation ID: {}", e.what());
                }
            }
            else if (std::holds_alternative<rtc::binary>(data)) {
                // Binary message (WorldData push) - no correlation ID.
                const auto& binaryData = std::get<rtc::binary>(data);
                spdlog::debug(
                    "WebSocketClient: Received binary push ({} bytes)", binaryData.size());

                // Optimization: Skip processing if no callback registered.
                // Binary WorldData pushes are unsolicited and only needed for async callbacks.
                // Benchmark mode doesn't use callbacks, so skip expensive JSON conversion.
                if (!messageCallback_) {
                    spdlog::trace("WebSocketClient: Dropping binary push (no callback)");
                    return;
                }

                timers_.startTimer("binary_worlddata_processing");

                try {
                    timers_.startTimer("binary_deserialize");
                    // Unpack binary to WorldData using zpp_bits.
                    WorldData worldData;
                    zpp::bits::in in(binaryData);
                    in(worldData).or_throw();
                    timers_.stopTimer("binary_deserialize");

                    timers_.startTimer("json_conversion");
                    // Convert to JSON string for MessageParser compatibility.
                    nlohmann::json doc;
                    doc["value"] = ReflectSerializer::to_json(worldData);
                    message = doc.dump();
                    timers_.stopTimer("json_conversion");
                }
                catch (const std::exception& e) {
                    spdlog::error("WebSocketClient: Failed to decode binary: {}", e.what());
                    timers_.stopTimer("binary_worlddata_processing");
                    return;
                }

                timers_.stopTimer("binary_worlddata_processing");
            }

            // Route message based on correlation ID.
            if (correlationId.has_value()) {
                // This is a response to a specific request - route to pending request.
                std::lock_guard<std::mutex> lock(pendingMutex_);
                auto it = pendingRequests_.find(*correlationId);
                if (it != pendingRequests_.end()) {
                    auto& pending = it->second;
                    std::lock_guard<std::mutex> reqLock(pending->mutex);
                    pending->response = message;
                    pending->received = true;
                    pending->cv.notify_one();
                    spdlog::debug(
                        "WebSocketClient: Routed response to pending request {}", *correlationId);
                }
                else {
                    spdlog::warn(
                        "WebSocketClient: Received response for unknown ID: {}", *correlationId);
                }
            }
            else {
                // No correlation ID - this is a notification or legacy blocking mode.
                // For legacy blocking mode (sendAndReceive without ID).
                response_ = message;
                responseReceived_ = true;

                // For async mode (callbacks).
                if (messageCallback_) {
                    messageCallback_(message);
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
            connectionFailed_ = true; // Mark as failed for connect() loop.
            if (disconnectedCallback_) {
                disconnectedCallback_();
            }
        });

        // Set up error handler.
        ws_->onError([this](std::string error) {
            spdlog::error("WebSocketClient error: {}", error);
            connectionFailed_ = true; // Mark as failed for connect() loop.
            if (errorCallback_) {
                errorCallback_(error);
            }
        });

        // Open connection.
        ws_->open(url);

        // Wait for connection (with timeout).
        auto startTime = std::chrono::steady_clock::now();
        while (!ws_->isOpen() && !connectionFailed_) {
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() > 5) {
                spdlog::error("WebSocketClient: Connection timeout");
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Check if connection succeeded or failed.
        if (connectionFailed_) {
            spdlog::debug("WebSocketClient: Connection failed (detected early)");
            return false;
        }

        spdlog::debug("WebSocketClient: Connected");
        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("WebSocketClient: Connection failed: {}", e.what());
        return false;
    }
}

std::string WebSocketClient::sendAndReceive(const std::string& message, int timeoutMs)
{
    if (!ws_ || !ws_->isOpen()) {
        spdlog::error("WebSocketClient: Not connected");
        return "";
    }

    // Generate unique correlation ID.
    uint64_t id = nextId_.fetch_add(1);

    // Inject ID into message JSON.
    std::string messageWithId;
    try {
        nlohmann::json json = nlohmann::json::parse(message);
        json["id"] = id;
        messageWithId = json.dump();
    }
    catch (const std::exception& e) {
        spdlog::error("WebSocketClient: Failed to inject correlation ID: {}", e.what());
        return "";
    }

    // Create pending request.
    auto pending = std::make_shared<PendingRequest>();
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingRequests_[id] = pending;
    }

    // Send message with correlation ID.
    spdlog::debug("WebSocketClient: Sending (id={}): {}", id, messageWithId);
    ws_->send(messageWithId);

    // Wait for response with matching ID.
    std::unique_lock<std::mutex> reqLock(pending->mutex);
    bool received = pending->cv.wait_for(
        reqLock, std::chrono::milliseconds(timeoutMs), [&pending]() { return pending->received; });

    // Clean up pending request.
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingRequests_.erase(id);
    }

    if (!received) {
        spdlog::error("WebSocketClient: Response timeout for ID {}", id);
        return "";
    }

    spdlog::debug(
        "WebSocketClient: Received response for ID {} ({} bytes)", id, pending->response.size());
    return pending->response;
}

bool WebSocketClient::send(const std::string& message)
{
    if (!ws_ || !ws_->isOpen()) {
        spdlog::error("WebSocketClient: Cannot send, not connected");
        return false;
    }

    try {
        spdlog::debug("WebSocketClient: Sending: {}", message);
        ws_->send(message);
        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("WebSocketClient: Send failed: {}", e.what());
        return false;
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

void WebSocketClient::onMessage(MessageCallback callback)
{
    messageCallback_ = callback;
}

void WebSocketClient::onConnected(ConnectionCallback callback)
{
    connectedCallback_ = callback;
}

void WebSocketClient::onDisconnected(ConnectionCallback callback)
{
    disconnectedCallback_ = callback;
}

void WebSocketClient::onError(ErrorCallback callback)
{
    errorCallback_ = callback;
}

} // namespace Client
} // namespace DirtSim
