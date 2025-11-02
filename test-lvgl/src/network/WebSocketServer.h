#pragma once

#include "../StateMachineInterface.h"
#include "CommandDeserializerJson.h"
#include "ResponseSerializerJson.h"
#include <memory>
#include <rtc/rtc.hpp>
#include <string>

namespace DirtSim {

/**
 * @brief WebSocket server for remote simulation control.
 *
 * Listens for WebSocket connections, deserializes JSON commands,
 * wraps them in Cwcs with response callbacks, and queues them to
 * the state machine for processing.
 */
class WebSocketServer {
public:
    /**
     * @brief Construct WebSocket server.
     * @param stateMachine The state machine to send events to.
     * @param port The port to listen on.
     */
    explicit WebSocketServer(StateMachineInterface& stateMachine, uint16_t port = 8080);

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

private:
    StateMachineInterface& stateMachine_;
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
};

} // namespace DirtSim
