#include "WebSocketClient.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

namespace DirtSim {
namespace Ui {

WebSocketClient::WebSocketClient()
{
}

WebSocketClient::~WebSocketClient()
{
    disconnect();
}

bool WebSocketClient::connect(const std::string& url)
{
    try {
        // IMPORTANT: Disconnect any existing connection first to prevent duplicate message handlers.
        if (ws_ && ws_->isOpen()) {
            spdlog::warn("UI WebSocketClient: Disconnecting existing connection before reconnecting");
            ws_->close();
        }
        ws_.reset();  // Release old WebSocket to ensure cleanup.

        spdlog::info("UI WebSocketClient: Connecting to {}", url);

        // Create WebSocket configuration.
        rtc::WebSocketConfiguration config;
        config.maxMessageSize = 10 * 1024 * 1024; // 10MB limit for WorldData JSON.

        // Create WebSocket.
        ws_ = std::make_shared<rtc::WebSocket>(config);

        // Set up message handler.
        ws_->onMessage([this](std::variant<rtc::binary, rtc::string> data) {
            if (std::holds_alternative<rtc::string>(data)) {
                std::string message = std::get<rtc::string>(data);
                spdlog::debug("UI WebSocketClient: Received message (length: {})", message.length());
                if (messageCallback_) {
                    spdlog::trace("UI WebSocketClient: Calling messageCallback_");
                    messageCallback_(message);
                } else {
                    spdlog::warn("UI WebSocketClient: Received message but no messageCallback_ set!");
                }
            }
        });

        // Set up open handler.
        ws_->onOpen([this]() {
            spdlog::info("UI WebSocketClient: Connection opened");
            if (connectedCallback_) {
                connectedCallback_();
            }
        });

        // Set up close handler.
        ws_->onClosed([this]() {
            spdlog::info("UI WebSocketClient: Connection closed");
            if (disconnectedCallback_) {
                disconnectedCallback_();
            }
        });

        // Set up error handler.
        ws_->onError([this](std::string error) {
            spdlog::error("UI WebSocketClient error: {}", error);
            if (errorCallback_) {
                errorCallback_(error);
            }
        });

        // Open connection.
        ws_->open(url);

        spdlog::info("UI WebSocketClient: Connection initiated");
        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("UI WebSocketClient: Connection failed: {}", e.what());
        return false;
    }
}

bool WebSocketClient::send(const std::string& message)
{
    if (!ws_ || !ws_->isOpen()) {
        spdlog::error("UI WebSocketClient: Cannot send, not connected");
        return false;
    }

    try {
        spdlog::debug("UI WebSocketClient: Sending: {}", message);
        ws_->send(message);
        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("UI WebSocketClient: Send failed: {}", e.what());
        return false;
    }
}

void WebSocketClient::disconnect()
{
    if (ws_) {
        if (ws_->isOpen()) {
            spdlog::info("UI WebSocketClient: Disconnecting");
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

} // namespace Ui
} // namespace DirtSim
