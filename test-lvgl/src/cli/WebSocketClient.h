#pragma once

#include <rtc/rtc.hpp>
#include <functional>
#include <memory>
#include <string>

namespace DirtSim {
namespace Client {

/**
 * @brief Simple WebSocket client for sending commands and receiving responses.
 *
 * Single-request client: connects, sends one message, waits for response, disconnects.
 */
class WebSocketClient {
public:
    WebSocketClient();
    ~WebSocketClient();

    /**
     * @brief Connect to WebSocket server.
     * @param url WebSocket URL (e.g., "ws://localhost:8080")
     * @return true if connected successfully.
     */
    bool connect(const std::string& url);

    /**
     * @brief Send a message and wait for response.
     * @param message JSON message to send.
     * @param timeoutMs Timeout in milliseconds (default: 5000).
     * @return Response string, or empty string on timeout/error.
     */
    std::string sendAndReceive(const std::string& message, int timeoutMs = 5000);

    /**
     * @brief Disconnect from server.
     */
    void disconnect();

private:
    std::shared_ptr<rtc::WebSocket> ws_;
    std::string response_;
    bool responseReceived_;
};

} // namespace Client
} // namespace DirtSim
