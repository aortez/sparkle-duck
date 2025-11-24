#pragma once

#include "server/api/ApiCommand.h"
#include "server/api/CellGet.h"
#include "server/api/CellSet.h"
#include "server/api/DiagramGet.h"
#include "server/api/Exit.h"
#include "server/api/GravitySet.h"
#include "server/api/PerfStatsGet.h"
#include "server/api/PhysicsSettingsGet.h"
#include "server/api/PhysicsSettingsSet.h"
#include "server/api/Reset.h"
#include "server/api/ScenarioConfigSet.h"
#include "server/api/SimRun.h"
#include "server/api/StateGet.h"
#include "server/api/TimerStatsGet.h"
#include "server/api/WorldResize.h"
#include <nlohmann/json.hpp>
#include <string>

namespace DirtSim {
namespace Server {

/**
 * @brief Serializes API response objects into JSON strings.
 *
 * Pure serialization - converts Response objects to JSON without
 * any side effects. Uses templates to handle different response types.
 */
class ResponseSerializerJson {
public:
    /**
     * @brief Serialize any API response into JSON string.
     * @tparam Response The response type to serialize.
     * @param response The response to serialize (moved).
     * @return JSON string representation.
     */
    template <typename Response>
    std::string serialize(Response&& response)
    {
        nlohmann::json doc;

        // Check if response is error or value.
        if (response.isError()) {
            // Error response: {"error": "message"}.
            doc["error"] = response.errorValue().message;
        }
        else {
            const auto& value = response.value();

            // Check if it's monostate (empty response).
            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::monostate>) {
                // Empty response - extract name from Response type metadata.
                // For now, we'll omit response_type for empty responses since they're rare.
                // We could add a static name() to each Command type if needed.
                doc["value"] = nlohmann::json::object();
            }
            else {
                // Has data - auto-extract response_type from Okay::name().
                doc["response_type"] = value.name();
                doc["value"] = value.toJson();
            }
        }

        return doc.dump();
    }
};

} // namespace Server
} // namespace DirtSim
