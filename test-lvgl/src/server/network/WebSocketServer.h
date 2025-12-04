#pragma once

#include "CommandDeserializerJson.h"
#include "ResponseSerializerJson.h"
#include "core/RenderMessage.h"
#include "core/StateMachineInterface.h"
#include "core/WorldData.h"
#include "core/network/BinaryProtocol.h"
#include "server/Event.h"
#include <algorithm>
#include <map>
#include <memory>
#include <rtc/rtc.hpp>
#include <string>
#include <vector>

namespace DirtSim {

class World;

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
     * @brief Broadcast World state as RenderMessage with per-client format.
     * @param world World instance to extract data and bones from.
     *
     * Each client receives RenderMessage in their requested format (BASIC or DEBUG).
     */
    void broadcastRenderMessage(const World& world);

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
     * @brief Handle incoming text message from a client (JSON protocol).
     * @param ws The WebSocket connection.
     * @param message The received message.
     */
    void onMessage(std::shared_ptr<rtc::WebSocket> ws, const std::string& message);

    /**
     * @brief Handle incoming binary message from a client (zpp_bits protocol).
     * @param ws The WebSocket connection.
     * @param data The received binary data.
     */
    void onBinaryMessage(std::shared_ptr<rtc::WebSocket> ws, const rtc::binary& data);

    /**
     * @brief Wrap ApiCommand in appropriate Cwc with JSON response callback.
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
     * @brief Wrap ApiCommand in appropriate Cwc with binary response callback.
     * @param command The command to wrap.
     * @param ws The WebSocket connection for sending response.
     * @param correlationId Correlation ID from request envelope.
     * @return Event variant containing the Cwc.
     */
    Event createCwcForCommandBinary(
        const ApiCommand& command, std::shared_ptr<rtc::WebSocket> ws, uint64_t correlationId);

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

    // =========================================================================
    // Binary protocol immediate handlers.
    // =========================================================================

    /**
     * @brief Handle state_get immediately with binary response.
     * @param ws The WebSocket connection for sending response.
     * @param correlationId Correlation ID from request envelope.
     */
    void handleStateGetImmediateBinary(std::shared_ptr<rtc::WebSocket> ws, uint64_t correlationId);

    /**
     * @brief Handle status_get immediately with binary response.
     * @param ws The WebSocket connection for sending response.
     * @param correlationId Correlation ID from request envelope.
     */
    void handleStatusGetImmediateBinary(std::shared_ptr<rtc::WebSocket> ws, uint64_t correlationId);

    /**
     * @brief Handle render_format_set immediately with binary response.
     * @param ws The WebSocket connection for sending response.
     * @param cmd The render format set command.
     * @param correlationId Correlation ID from request envelope.
     */
    void handleRenderFormatSetImmediateBinary(
        std::shared_ptr<rtc::WebSocket> ws,
        const Api::RenderFormatSet::Command& cmd,
        uint64_t correlationId);
};

} // namespace Server
} // namespace DirtSim
