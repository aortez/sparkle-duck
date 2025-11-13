#include "MessageParser.h"
#include "core/PhysicsSettings.h"
#include "core/WorldData.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

std::optional<Event> MessageParser::parse(const std::string& message)
{
    try {
        nlohmann::json json = nlohmann::json::parse(message);

        // Type 1: Notifications (proactive from server).
        if (json.contains("type")) {
            return parseFrameReady(json);
        }

        // Type 2: Error responses.
        if (json.contains("error")) {
            handleError(json);
            return std::nullopt; // Error responses don't generate events.
        }

        // Type 3: Success responses with data.
        if (json.contains("value")) {
            return parseWorldDataResponse(json);
        }

        spdlog::warn("MessageParser: Unknown message format: {}", message);
        return std::nullopt;
    }
    catch (const std::exception& e) {
        spdlog::error("MessageParser: Failed to parse message: {}", e.what());
        spdlog::debug("MessageParser: Invalid message: {}", message);
        return std::nullopt;
    }
}

std::optional<Event> MessageParser::parseFrameReady(const nlohmann::json& json)
{
    if (json["type"] == "frame_ready") {
        uint64_t stepNumber = json.value("stepNumber", 0ULL);
        int64_t timestamp = json.value("timestamp", 0LL);

        spdlog::debug("MessageParser: Parsed frame_ready (step {})", stepNumber);

        return FrameReadyNotification{ stepNumber, timestamp };
    }

    spdlog::warn("MessageParser: Unknown notification type: {}", json["type"].get<std::string>());
    return std::nullopt;
}

std::optional<Event> MessageParser::parseWorldDataResponse(const nlohmann::json& json)
{
    const auto& value = json["value"];

    // Check if this is a WorldData response (contains width and cells).
    if (value.contains("width") && value.contains("cells")) {
        // Automatic deserialization via ADL functions!
        WorldData worldData = value.get<WorldData>();

        // Create UiUpdateEvent with the received data.
        uint64_t stepCount = worldData.timestep;
        double fps = worldData.fps_server;
        UiUpdateEvent evt{ .sequenceNum = 0,
                           .worldData = std::move(worldData),
                           .fps = fps,
                           .stepCount = stepCount,
                           .isPaused = false,
                           .timestamp = std::chrono::steady_clock::now() };

        return evt;
    }

    // Check if this is a PhysicsSettings response (contains gravity, elasticity, etc.).
    if (value.contains("gravity") && value.contains("elasticity")) {
        // Automatic deserialization via ADL functions!
        PhysicsSettings settings = value.get<PhysicsSettings>();

        spdlog::info("MessageParser: Parsed PhysicsSettings (gravity={:.2f})", settings.gravity);

        return PhysicsSettingsReceivedEvent{ settings };
    }

    // Other response types (empty success, etc.) - just log.
    spdlog::debug("MessageParser: Received generic response: {}", value.dump());
    return std::nullopt;
}

void MessageParser::handleError(const nlohmann::json& json)
{
    std::string errorMsg = json["error"].get<std::string>();
    spdlog::error("MessageParser: DSSM error: {}", errorMsg);
    // TODO: Could queue an ErrorEvent here if we add one.
}

} // namespace Ui
} // namespace DirtSim
