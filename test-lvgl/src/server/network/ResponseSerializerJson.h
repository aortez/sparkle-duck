#pragma once

#include "../api/ApiCommand.h"
#include "lvgl/src/libs/thorvg/rapidjson/stringbuffer.h"
#include "lvgl/src/libs/thorvg/rapidjson/writer.h"
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
        rapidjson::Document doc(rapidjson::kObjectType);
        auto& allocator = doc.GetAllocator();

        // Check if response is error or value.
        if (response.isError()) {
            // Error response: {"error": "message"}.
            rapidjson::Value errorVal(response.error().message.c_str(), allocator);
            doc.AddMember("error", errorVal, allocator);
        }
        else {
            using T = std::decay_t<Response>;

            // Success response: {"value": {...}}.
            if constexpr (std::is_same_v<T, Api::CellGet::Response>) {
                rapidjson::Value cellJson = response.value().cell.toJson(allocator);
                doc.AddMember("value", cellJson, allocator);
            }
            else if constexpr (std::is_same_v<T, Api::StateGet::Response>) {
                rapidjson::Document worldJson = response.value().world.toJSON();
                rapidjson::Value worldVal(rapidjson::kObjectType);
                worldVal.CopyFrom(worldJson, allocator);
                doc.AddMember("value", worldVal, allocator);
            }
            else if constexpr (std::is_same_v<T, Api::StepN::Response>) {
                rapidjson::Value valueObj(rapidjson::kObjectType);
                valueObj.AddMember("timestep", response.value().timestep, allocator);
                doc.AddMember("value", valueObj, allocator);
            }
            else if constexpr (
                std::is_same_v<T, Api::CellSet::Response>
                || std::is_same_v<T, Api::GravitySet::Response>
                || std::is_same_v<T, Api::Reset::Response>) {
                rapidjson::Value valueObj(rapidjson::kObjectType);
                doc.AddMember("value", valueObj, allocator);
            }
        }

        return documentToString(doc);
    }

    /**
     * @brief Helper to convert rapidjson::Document to JSON string.
     * @param doc The document to serialize.
     * @return JSON string.
     */
    static std::string documentToString(const rapidjson::Document& doc)
    {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);
        return buffer.GetString();
    }
};

} // namespace Server
} // namespace DirtSim
