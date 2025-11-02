#pragma once

#include "../../../server/api/ApiError.h"
#include "../../../core/Result.h"
#include <nlohmann/json.hpp>
#include <string>
#include <variant>

namespace DirtSim {
namespace Ui {

/**
 * @brief Serializes API responses to JSON strings.
 *
 * Pure serialization - converts Response objects to JSON without
 * any side effects. Does not know about network layers or callbacks.
 */
class ResponseSerializerJson {
public:
    /**
     * @brief Serialize a Response<T, ApiError> to JSON string.
     * @param response The response to serialize.
     * @return JSON string representation.
     */
    template <typename OkayType>
    std::string serialize(const Result<OkayType, ApiError>& response)
    {
        nlohmann::json j;

        if (response.isValue()) {
            // Success response.
            if constexpr (std::is_same_v<OkayType, std::monostate>) {
                // No data to return, just success.
                j = nlohmann::json{{"success", true}};
            }
            else {
                // Return data using toJson().
                j = response.value().toJson();
                j["success"] = true;
            }
        }
        else {
            // Error response.
            j = nlohmann::json{
                {"success", false},
                {"error", response.error().message}
            };
        }

        return j.dump();
    }
};

} // namespace Ui
} // namespace DirtSim
