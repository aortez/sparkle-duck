#pragma once

#include "../api/ApiCommand.h"
#include "../api/CellGet.h"
#include "../api/CellSet.h"
#include "../api/GravitySet.h"
#include "../api/Reset.h"
#include "../api/StateGet.h"
#include "../api/StepN.h"
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
            doc["error"] = response.error().message;
        }
        else {
            using T = std::decay_t<Response>;

            // Success response: {"value": {...}}.
            if constexpr (std::is_same_v<T, Api::CellGet::Response>) {
                doc["value"] = response.value().toJson();
            }
            else if constexpr (std::is_same_v<T, Api::StateGet::Response>) {
                doc["value"] = response.value().toJson();
            }
            else if constexpr (std::is_same_v<T, Api::StepN::Response>) {
                doc["value"] = response.value().toJson();
            }
            else if constexpr (
                std::is_same_v<T, Api::CellSet::Response>
                || std::is_same_v<T, Api::GravitySet::Response>
                || std::is_same_v<T, Api::Reset::Response>) {
                // Empty object for commands with no response data.
                doc["value"] = nlohmann::json::object();
            }
        }

        return doc.dump();
    }
};

} // namespace Server
} // namespace DirtSim
