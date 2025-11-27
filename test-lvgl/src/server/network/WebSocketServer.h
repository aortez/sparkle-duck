#pragma once

#include "CommandDeserializerJson.h"
#include "ResponseSerializerJson.h"
#include "core/RenderMessage.h"
#include "core/StateMachineInterface.h"
#include "core/WorldData.h"
#include "server/Event.h"
#include <algorithm>
#include <map>
#include <memory>
#include <rtc/rtc.hpp>
#include <string>
#include <vector>

namespace DirtSim {
namespace Server {

class WebSocketServer {
public:
    explicit WebSocketServer(
        DirtSim::StateMachineInterface<Event>& stateMachine, uint16_t port = 8080);

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

    /**
     * @brief Broadcast binary data to all connected clients.
     * @param data Binary data to broadcast.
     */
    void broadcastBinary(const rtc::binary& data);

    /**
     * @brief Broadcast WorldData as RenderMessage with per-client format.
     * @param data World data to pack and send.
     *
     * Each client receives RenderMessage in their requested format (BASIC or DEBUG).
     */
    void broadcastRenderMessage(const WorldData& data);

    /**
     * @brief Set render format for a specific client.
     * @param ws Client connection.
     * @param format Render format to use.
     */
    void setClientRenderFormat(std::shared_ptr<rtc::WebSocket> ws, RenderFormat format);

    /**
     * @brief Get render format for a specific client (defaults to BASIC).
     * @param ws Client connection.
     * @return Current render format for this client.
     */
    RenderFormat getClientRenderFormat(std::shared_ptr<rtc::WebSocket> ws) const;

    // Public for generic Cwc creation helpers.
    ResponseSerializerJson serializer_;
    DirtSim::StateMachineInterface<Event>& stateMachine_;

private:
    std::vector<std::shared_ptr<rtc::WebSocket>> connectedClients_;
    std::unique_ptr<rtc::WebSocketServer> server_;
    CommandDeserializerJson deserializer_;

    // Per-client render format tracking (defaults to BASIC).
    std::map<std::shared_ptr<rtc::WebSocket>, RenderFormat> clientRenderFormats_;

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
     * @param correlationId Optional correlation ID from request.
     * @return Event variant containing the Cwc.
     */
    Event createCwcForCommand(
        const ApiCommand& command,
        std::shared_ptr<rtc::WebSocket> ws,
        std::optional<uint64_t> correlationId);

    /**
     * @brief Handle state_get immediately without queuing (low latency path).
     * @param ws The WebSocket connection for sending response.
     * @param correlationId Optional correlation ID from request.
     */
    void handleStateGetImmediate(
        std::shared_ptr<rtc::WebSocket> ws, std::optional<uint64_t> correlationId);

    /**
     * @brief Handle status_get immediately without queuing (low latency path).
     * @param ws The WebSocket connection for sending response.
     * @param correlationId Optional correlation ID from request.
     */
    void handleStatusGetImmediate(
        std::shared_ptr<rtc::WebSocket> ws, std::optional<uint64_t> correlationId);

    /**
     * @brief Handle render_format_set immediately without queuing.
     * @param ws The WebSocket connection for sending response.
     * @param cmd The render format set command.
     * @param correlationId Optional correlation ID from request.
     */
    void handleRenderFormatSetImmediate(
        std::shared_ptr<rtc::WebSocket> ws,
        const Api::RenderFormatSet::Command& cmd,
        std::optional<uint64_t> correlationId);
};

} // namespace Server
} // namespace DirtSim
