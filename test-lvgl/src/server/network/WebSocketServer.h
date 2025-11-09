#pragma once

#include "core/StateMachineInterface.h"
#include "server/Event.h"
#include "CommandDeserializerJson.h"
#include "ResponseSerializerJson.h"
#include <algorithm>
#include <memory>
#include <rtc/rtc.hpp>
#include <string>
#include <vector>

namespace DirtSim {
namespace Server {

class WebSocketServer {
public:
    explicit WebSocketServer(DirtSim::StateMachineInterface<Event>& stateMachine, uint16_t port = 8080);

    /**
     * @brief Start the server.
     */
    void start();

    /**
     * @brief Stop the server.
     */
    void stop();

    /**
     * @brief Get the port the server is listening on.
     * @return Port number.
     */
    uint16_t getPort() const;

    /**
     * @brief Broadcast a message to all connected clients.
     * @param message JSON message to broadcast.
     */
    void broadcast(const std::string& message);

private:
    std::vector<std::shared_ptr<rtc::WebSocket>> connectedClients_;
    DirtSim::StateMachineInterface<Event>& stateMachine_;
    std::unique_ptr<rtc::WebSocketServer> server_;
    CommandDeserializerJson deserializer_;
    ResponseSerializerJson serializer_;

    /**
     * @brief Handle new WebSocket connection.
     * @param ws The new WebSocket connection.
     */
    void onClientConnected(std::shared_ptr<rtc::WebSocket> ws);

    /**
     * @brief Handle incoming message from a client.
     * @param ws The WebSocket connection.
     * @param message The received message.
     */
    void onMessage(std::shared_ptr<rtc::WebSocket> ws, const std::string& message);

    /**
     * @brief Wrap ApiCommand in appropriate Cwc with response callback.
     * @param command The command to wrap.
     * @param ws The WebSocket connection for sending response.
     * @return Event variant containing the Cwc.
     */
    Event createCwcForCommand(const ApiCommand& command, std::shared_ptr<rtc::WebSocket> ws);

    /**
     * @brief Handle state_get immediately without queuing (low latency path).
     * @param ws The WebSocket connection for sending response.
     */
    void handleStateGetImmediate(std::shared_ptr<rtc::WebSocket> ws);
};

} // namespace Server
} // namespace DirtSim
