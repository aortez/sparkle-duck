#pragma once

#include <rtc/rtc.hpp>
#include <functional>
#include <memory>
#include <string>

namespace DirtSim {
namespace Client {

/**
 * @brief WebSocket client supporting both blocking (sendAndReceive) and async (callbacks) modes.
 */
class WebSocketClient {
public:
    using MessageCallback = std::function<void(const std::string&)>;
    using ConnectionCallback = std::function<void()>;
    using ErrorCallback = std::function<void(const std::string&)>;

    WebSocketClient();
    ~WebSocketClient();

    bool connect(const std::string& url);
    bool send(const std::string& message);
    std::string sendAndReceive(const std::string& message, int timeoutMs = 5000);
    void disconnect();
    bool isConnected() const;

    void onMessage(MessageCallback callback);
    void onConnected(ConnectionCallback callback);
    void onDisconnected(ConnectionCallback callback);
    void onError(ErrorCallback callback);

private:
    std::shared_ptr<rtc::WebSocket> ws_;
    std::string response_;
    bool responseReceived_;
    MessageCallback messageCallback_;
    ConnectionCallback connectedCallback_;
    ConnectionCallback disconnectedCallback_;
    ErrorCallback errorCallback_;
};

} // namespace Client
} // namespace DirtSim
