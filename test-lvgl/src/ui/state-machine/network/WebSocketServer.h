#pragma once

#include "CommandDeserializerJson.h"
#include "ResponseSerializerJson.h"
#include "core/StateMachineInterface.h"
#include "ui/state-machine/Event.h"

#include <chrono>
#include <memory>
#include <rtc/rtc.hpp>
#include <string>

namespace DirtSim {
namespace Ui {

class WebSocketServer {
public:
    explicit WebSocketServer(
        DirtSim::StateMachineInterface<Event>& stateMachine, uint16_t port = 7070);

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
     * @param clientLastScreenshot Per-client timestamp for screenshot throttling.
     */
    void onMessage(
        std::shared_ptr<rtc::WebSocket> ws,
        const std::string& message,
        std::shared_ptr<std::chrono::steady_clock::time_point> clientLastScreenshot);

    /**
     * @brief Wrap UiApiCommand in appropriate Cwc with response callback.
     * @param command The command to wrap.
     * @param ws The WebSocket connection for sending response.
     * @param correlationId Optional correlation ID to include in response.
     * @return Event variant containing the Cwc.
     */
    Event createCwcForCommand(
        const UiApiCommand& command,
        std::shared_ptr<rtc::WebSocket> ws,
        std::optional<uint64_t> correlationId);
};

} // namespace Ui
} // namespace DirtSim
