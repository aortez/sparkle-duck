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

WebSocketClient::WebSocketClient() : responseReceived_(false)
{}

WebSocketClient::~WebSocketClient()
{
    disconnect();
}

bool WebSocketClient::connect(const std::string& url)
{
    try {
        spdlog::debug("WebSocketClient: Connecting to {}", url);

        // Create WebSocket with large message size for WorldData.
        rtc::WebSocketConfiguration config;
        config.maxMessageSize = 10 * 1024 * 1024; // 10MB limit for WorldData JSON.

        ws_ = std::make_shared<rtc::WebSocket>(config);

        // Set up message handler (supports both blocking and async modes, JSON and CBOR).
        ws_->onMessage([this](std::variant<rtc::binary, rtc::string> data) {
            std::string message;

            if (std::holds_alternative<rtc::string>(data)) {
                // JSON string message.
                message = std::get<rtc::string>(data);
                spdlog::debug("WebSocketClient: Received JSON response ({} bytes)", message.size());
            }
            else if (std::holds_alternative<rtc::binary>(data)) {
                // zpp_bits binary message - unpack WorldData directly.
                const auto& binaryData = std::get<rtc::binary>(data);
                spdlog::debug(
                    "WebSocketClient: Received binary response ({} bytes)", binaryData.size());

                try {
                    // Unpack binary to WorldData using zpp_bits.
                    WorldData worldData;
                    zpp::bits::in in(binaryData);
                    in(worldData).or_throw();

                    // Convert to JSON string for MessageParser compatibility.
                    nlohmann::json doc;
                    doc["value"] = ReflectSerializer::to_json(worldData);
                    message = doc.dump();
                }
                catch (const std::exception& e) {
                    spdlog::error("WebSocketClient: Failed to decode binary: {}", e.what());
                    return;
                }
            }

            // For blocking mode (sendAndReceive).
            response_ = message;
            responseReceived_ = true;

            // For async mode (callbacks).
            if (messageCallback_) {
                messageCallback_(message);
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
            if (disconnectedCallback_) {
                disconnectedCallback_();
            }
        });

        // Set up error handler.
        ws_->onError([this](std::string error) {
            spdlog::error("WebSocketClient error: {}", error);
            if (errorCallback_) {
                errorCallback_(error);
            }
        });

        // Open connection.
        ws_->open(url);

        // Wait for connection (with timeout).
        auto startTime = std::chrono::steady_clock::now();
        while (!ws_->isOpen()) {
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() > 5) {
                spdlog::error("WebSocketClient: Connection timeout");
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
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

    // Reset response state.
    response_.clear();
    responseReceived_ = false;

    // Send message.
    spdlog::debug("WebSocketClient: Sending: {}", message);
    ws_->send(message);

    // Wait for response.
    auto startTime = std::chrono::steady_clock::now();
    while (!responseReceived_) {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeoutMs) {
            spdlog::error("WebSocketClient: Response timeout");
            return "";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return response_;
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
