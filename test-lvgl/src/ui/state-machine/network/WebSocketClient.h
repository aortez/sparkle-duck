#pragma once

#include <rtc/rtc.hpp>
#include <functional>
#include <memory>
#include <string>

namespace DirtSim {
namespace Ui {

/**
 * @brief Persistent WebSocket client for connecting to DSSM server.
 *
 * Maintains connection, sends commands, and receives world updates.
 */
class WebSocketClient {
public:
    using MessageCallback = std::function<void(const std::string&)>;
    using ConnectionCallback = std::function<void()>;
    using ErrorCallback = std::function<void(const std::string&)>;

    WebSocketClient();
    ~WebSocketClient();

    /**
     * @brief Connect to WebSocket server.
     * @param url WebSocket URL (e.g., "ws://localhost:8080")
     * @return true if connection initiated successfully.
     */
    bool connect(const std::string& url);

    /**
     * @brief Send a message to the server.
     * @param message JSON message to send.
     * @return true if sent successfully.
     */
    bool send(const std::string& message);

    /**
     * @brief Disconnect from server.
     */
    void disconnect();

    /**
     * @brief Check if connected.
     * @return true if WebSocket is open.
     */
    bool isConnected() const;

    /**
     * @brief Set callback for received messages.
     * @param callback Function called when message received.
     */
    void onMessage(MessageCallback callback);

    /**
     * @brief Set callback for connection established.
     * @param callback Function called when connection opens.
     */
    void onConnected(ConnectionCallback callback);

    /**
     * @brief Set callback for disconnection.
     * @param callback Function called when connection closes.
     */
    void onDisconnected(ConnectionCallback callback);

    /**
     * @brief Set callback for errors.
     * @param callback Function called on error.
     */
    void onError(ErrorCallback callback);

private:
    std::shared_ptr<rtc::WebSocket> ws_;
    MessageCallback messageCallback_;
    ConnectionCallback connectedCallback_;
    ConnectionCallback disconnectedCallback_;
    ErrorCallback errorCallback_;
};

} // namespace Ui
} // namespace DirtSim
