#pragma once

#include "../Event.h"
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace DirtSim {
namespace Ui {

/**
 * @brief Parses WebSocket messages from DSSM server into UI events.
 *
 * This is a state-independent message parser that converts JSON messages
 * into strongly-typed events for the UI state machine. It handles both
 * notifications (frame_ready) and responses (state_get, errors).
 */
class MessageParser {
public:
    /**
     * @brief Parse a WebSocket message into a UI event.
     * @param message Raw JSON string from DSSM server.
     * @return Parsed event if successful, nullopt if unknown/invalid message.
     */
    static std::optional<Event> parse(const std::string& message);

private:
    /**
     * @brief Try to parse as a frame_ready notification.
     */
    static std::optional<Event> parseFrameReady(const nlohmann::json& json);

    /**
     * @brief Try to parse as a state_get response with WorldData.
     */
    static std::optional<Event> parseWorldDataResponse(const nlohmann::json& json);

    /**
     * @brief Handle error responses.
     */
    static void handleError(const nlohmann::json& json);
};

} // namespace Ui
} // namespace DirtSim
