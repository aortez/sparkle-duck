#include "WebSocketClient.h"
#include "core/MsgPackAdapter.h"
#include "core/ReflectSerializer.h"
#include "core/RenderMessage.h"
#include "core/RenderMessageUtils.h"
#include "core/WorldData.h"
#include "core/api/UiUpdateEvent.h"
#include <chrono>
#include <spdlog/spdlog.h>
#include <thread>
#include <zpp_bits.h>

namespace DirtSim {
namespace Ui {

WebSocketClient::WebSocketClient()
{}

WebSocketClient::~WebSocketClient()
{
    disconnect();
}

void WebSocketClient::setEventSink(EventSink* sink)
{
    eventSink_ = sink;
}

bool WebSocketClient::connect(const std::string& url)
{
    try {
        // IMPORTANT: Disconnect any existing connection first to prevent duplicate message
        // handlers.
        if (ws_ && ws_->isOpen()) {
            spdlog::warn(
                "UI WebSocketClient: Disconnecting existing connection before reconnecting");
            ws_->close();
        }
        ws_.reset(); // Release old WebSocket to ensure cleanup.

        spdlog::info("UI WebSocketClient: Connecting to {}", url);

        // Create WebSocket configuration.
        rtc::WebSocketConfiguration config;
        config.maxMessageSize = 10 * 1024 * 1024; // 10MB limit for WorldData JSON.

        // Create WebSocket.
        ws_ = std::make_shared<rtc::WebSocket>(config);

        // Set up message handler (handles both JSON string and MessagePack binary).
        ws_->onMessage([this](std::variant<rtc::binary, rtc::string> data) {
            std::string message;

            if (std::holds_alternative<rtc::string>(data)) {
                // JSON string message.
                message = std::get<rtc::string>(data);
                spdlog::debug(
                    "UI WebSocketClient: Received JSON message (length: {})", message.length());
            }
            else if (std::holds_alternative<rtc::binary>(data)) {
                // zpp_bits binary message - unpack RenderMessage.
                const auto& binaryData = std::get<rtc::binary>(data);
                spdlog::info(
                    "UI WebSocketClient: Received binary message ({} bytes)", binaryData.size());

                try {
                    // Unpack binary to RenderMessage using zpp_bits.
                    auto deserializeStart = std::chrono::steady_clock::now();
                    RenderMessage renderMsg;
                    spdlog::info("UI WebSocketClient: Deserializing RenderMessage...");
                    zpp::bits::in in(binaryData);
                    in(renderMsg).or_throw();
                    spdlog::info(
                        "UI WebSocketClient: Deserialized format={}, width={}, height={}",
                        static_cast<int>(renderMsg.format),
                        renderMsg.width,
                        renderMsg.height);
                    auto deserializeEnd = std::chrono::steady_clock::now();
                    auto deserializeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                             deserializeEnd - deserializeStart)
                                             .count();

                    // Reconstruct WorldData from RenderMessage.
                    WorldData worldData;
                    worldData.width = renderMsg.width;
                    worldData.height = renderMsg.height;
                    worldData.timestep = renderMsg.timestep;
                    worldData.fps_server = renderMsg.fps_server;
                    worldData.scenario_id = renderMsg.scenario_id;
                    worldData.scenario_config = renderMsg.scenario_config;
                    worldData.tree_vision = renderMsg.tree_vision;

                    // Unpack cells based on format.
                    size_t numCells = renderMsg.width * renderMsg.height;
                    worldData.cells.resize(numCells);

                    if (renderMsg.format == RenderFormat::BASIC) {
                        // Unpack BasicCell format.
                        const BasicCell* basicCells =
                            reinterpret_cast<const BasicCell*>(renderMsg.payload.data());
                        for (size_t i = 0; i < numCells; ++i) {
                            MaterialType material;
                            double fill_ratio;
                            RenderMessageUtils::unpackBasicCell(
                                basicCells[i], material, fill_ratio);

                            worldData.cells[i].material_type = material;
                            worldData.cells[i].fill_ratio = fill_ratio;
                            // Other fields remain at default values.
                        }
                    }
                    else if (renderMsg.format == RenderFormat::DEBUG) {
                        // Unpack DebugCell format.
                        const DebugCell* debugCells =
                            reinterpret_cast<const DebugCell*>(renderMsg.payload.data());
                        for (size_t i = 0; i < numCells; ++i) {
                            auto unpacked = RenderMessageUtils::unpackDebugCell(debugCells[i]);

                            worldData.cells[i].material_type = unpacked.material_type;
                            worldData.cells[i].fill_ratio = unpacked.fill_ratio;
                            worldData.cells[i].com = unpacked.com;
                            worldData.cells[i].velocity = unpacked.velocity;
                            worldData.cells[i].hydrostatic_component = unpacked.pressure_hydro;
                            worldData.cells[i].dynamic_component = unpacked.pressure_dynamic;
                            worldData.cells[i].pressure =
                                unpacked.pressure_hydro + unpacked.pressure_dynamic;
                        }
                    }

                    // Apply sparse organism data.
                    std::vector<uint8_t> organism_ids =
                        RenderMessageUtils::applyOrganismData(renderMsg.organisms, numCells);
                    for (size_t i = 0; i < numCells; ++i) {
                        worldData.cells[i].organism_id = organism_ids[i];
                    }

                    static int deserializeCount = 0;
                    static double totalDeserializeMs = 0.0;
                    deserializeCount++;
                    totalDeserializeMs += deserializeMs;
                    if (deserializeCount % 10000 == 0) {
                        spdlog::info(
                            "UI WebSocketClient: RenderMessage deserialization avg {:.1f}ms over "
                            "{} "
                            "frames (latest: {}ms, {} cells, format: {})",
                            totalDeserializeMs / deserializeCount,
                            deserializeCount,
                            deserializeMs,
                            worldData.cells.size(),
                            renderMsg.format == RenderFormat::BASIC ? "BASIC" : "DEBUG");
                    }

                    // Fast path: queue UiUpdateEvent directly via EventSink.
                    if (eventSink_) {
                        // No throttling needed - server pushes frames continuously.
                        auto now = std::chrono::steady_clock::now();
                        uint64_t stepCount = worldData.timestep;
                        UiUpdateEvent evt{ .sequenceNum = 0,
                                           .worldData = std::move(worldData),
                                           .fps = 0,
                                           .stepCount = stepCount,
                                           .isPaused = false,
                                           .timestamp = now };

                        eventSink_->queueEvent(evt);
                        spdlog::debug(
                            "UI WebSocketClient: Queued UiUpdateEvent (step {})", stepCount);
                        return; // Done - skip JSON conversion entirely.
                    }

                    // Legacy fallback: convert to JSON for MessageParser.
                    nlohmann::json doc;
                    doc["value"] = ReflectSerializer::to_json(worldData);
                    message = doc.dump();
                }
                catch (const std::exception& e) {
                    spdlog::error("UI WebSocketClient: Failed to decode binary: {}", e.what());
                    return;
                }
            }

            // Always store message for potential blocking mode.
            response_ = message;
            responseReceived_ = true;

            // Only call async callback if not waiting for blocking response.
            // If responseReceived was already true, we're processing a WorldData push.
            if (messageCallback_) {
                spdlog::trace("UI WebSocketClient: Calling messageCallback_");
                messageCallback_(message);
            }
        });

        // Set up open handler.
        ws_->onOpen([this]() {
            spdlog::info("UI WebSocketClient: Connection opened");
            if (connectedCallback_) {
                connectedCallback_();
            }
        });

        // Set up close handler.
        ws_->onClosed([this]() {
            spdlog::info("UI WebSocketClient: Connection closed");
            if (disconnectedCallback_) {
                disconnectedCallback_();
            }
        });

        // Set up error handler.
        ws_->onError([this](std::string error) {
            spdlog::error("UI WebSocketClient error: {}", error);
            if (errorCallback_) {
                errorCallback_(error);
            }
        });

        // Open connection.
        ws_->open(url);

        spdlog::info("UI WebSocketClient: Connection initiated");
        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("UI WebSocketClient: Connection failed: {}", e.what());
        return false;
    }
}

bool WebSocketClient::send(const std::string& message)
{
    if (!ws_ || !ws_->isOpen()) {
        spdlog::error("UI WebSocketClient: Cannot send, not connected");
        return false;
    }

    try {
        spdlog::debug("UI WebSocketClient: Sending: {}", message);
        ws_->send(message);
        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("UI WebSocketClient: Send failed: {}", e.what());
        return false;
    }
}

std::string WebSocketClient::sendAndReceive(const std::string& message, int timeoutMs)
{
    if (!ws_ || !ws_->isOpen()) {
        spdlog::error("UI WebSocketClient: Not connected");
        return "";
    }

    // Reset response state.
    response_.clear();
    responseReceived_ = false;

    // Send message.
    spdlog::debug("UI WebSocketClient: Sending and waiting for response: {}", message);
    ws_->send(message);

    // Wait for response.
    auto startTime = std::chrono::steady_clock::now();
    while (!responseReceived_) {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeoutMs) {
            spdlog::error("UI WebSocketClient: Response timeout");
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
            spdlog::info("UI WebSocketClient: Disconnecting");
            ws_->close();
        }
        ws_.reset();
    }
}

bool WebSocketClient::isConnected() const
{
    return ws_ && ws_->isOpen();
}

void WebSocketClient::onMessage(MessageCallback callback)
{
    messageCallback_ = callback;
}

void WebSocketClient::onConnected(ConnectionCallback callback)
{
    connectedCallback_ = callback;
}

void WebSocketClient::onDisconnected(ConnectionCallback callback)
{
    disconnectedCallback_ = callback;
}

void WebSocketClient::onError(ErrorCallback callback)
{
    errorCallback_ = callback;
}

} // namespace Ui
} // namespace DirtSim
