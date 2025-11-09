#pragma once

#include "core/WorldData.h"
#include "ui/state-machine/EventSink.h"
#include <functional>
#include <memory>
#include <rtc/rtc.hpp>
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

    void setEventSink(EventSink* sink);

    /**
     * @brief Connect to WebSocket server.
     * @param url WebSocket URL (e.g., "ws://localhost:8080")
     * @return true if connection initiated successfully.
     */
    bool connect(const std::string& url);

    /**
     * @brief Send a message to the server (async).
     * @param message JSON message to send.
     * @return true if sent successfully.
     */
    bool send(const std::string& message);

    /**
     * @brief Send a message and wait for response (blocking).
     * @param message JSON message to send.
     * @param timeoutMs Timeout in milliseconds.
     * @return Response string, or empty on timeout.
     */
    std::string sendAndReceive(const std::string& message, int timeoutMs = 5000);

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
     * @brief Set callback for received messages (legacy JSON messages).
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
    EventSink* eventSink_ = nullptr;
    MessageCallback messageCallback_;
    ConnectionCallback connectedCallback_;
    ConnectionCallback disconnectedCallback_;
    ErrorCallback errorCallback_;

    // For blocking sendAndReceive().
    std::string response_;
    bool responseReceived_ = false;

    // Frame dropping: throttle to 60 FPS max.
    std::chrono::steady_clock::time_point lastEventQueueTime_;
};

} // namespace Ui
} // namespace DirtSim
