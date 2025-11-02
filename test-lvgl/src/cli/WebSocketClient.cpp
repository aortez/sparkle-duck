#include "WebSocketClient.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

namespace DirtSim {
namespace Client {

WebSocketClient::WebSocketClient()
    : responseReceived_(false)
{
}

WebSocketClient::~WebSocketClient()
{
    disconnect();
}

bool WebSocketClient::connect(const std::string& url)
{
    try {
        spdlog::debug("WebSocketClient: Connecting to {}", url);

        // Create WebSocket.
        ws_ = std::make_shared<rtc::WebSocket>();

        // Set up message handler.
        ws_->onMessage([this](std::variant<rtc::binary, rtc::string> data) {
            if (std::holds_alternative<rtc::string>(data)) {
                response_ = std::get<rtc::string>(data);
                responseReceived_ = true;
                spdlog::debug("WebSocketClient: Received response");
            }
        });

        // Set up error handler.
        ws_->onError([](std::string error) {
            spdlog::error("WebSocketClient error: {}", error);
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

void WebSocketClient::disconnect()
{
    if (ws_) {
        if (ws_->isOpen()) {
            ws_->close();
        }
        ws_.reset();
    }
}

} // namespace Client
} // namespace DirtSim
