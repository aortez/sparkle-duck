#include "ResponseSerializerJson.h"
#include "lvgl/src/libs/thorvg/rapidjson/stringbuffer.h"
#include "lvgl/src/libs/thorvg/rapidjson/writer.h"

namespace DirtSim {

std::string ResponseSerializerJson::serialize(const ApiResponse& response)
{
    return std::visit(
        [this](auto&& resp) -> std::string {
            using T = std::decay_t<decltype(resp)>;

            rapidjson::Document doc(rapidjson::kObjectType);
            auto& allocator = doc.GetAllocator();

            // Check if response is error or value.
            if (resp.isError()) {
                // Error response: {"error": "message"}.
                rapidjson::Value errorVal(resp.errorValue().message.c_str(), allocator);
                doc.AddMember("error", errorVal, allocator);
            }
            else {
                // Success response: {"value": {...}}.
                if constexpr (std::is_same_v<T, Api::CellGet::Response>) {
                    // CellGet returns cell JSON.
                    rapidjson::Value valueObj(rapidjson::kObjectType);
                    valueObj.CopyFrom(resp.value().cellJson, allocator);
                    doc.AddMember("value", valueObj, allocator);
                }
                else if constexpr (std::is_same_v<T, Api::StateGet::Response>) {
                    // StateGet returns world JSON.
                    rapidjson::Value valueObj(rapidjson::kObjectType);
                    valueObj.CopyFrom(resp.value().worldJson, allocator);
                    doc.AddMember("value", valueObj, allocator);
                }
                else if constexpr (
                    std::is_same_v<T, Api::CellSet::Response>
                    || std::is_same_v<T, Api::GravitySet::Response>
                    || std::is_same_v<T, Api::Reset::Response>) {
                    // These return empty success: {"value": {}}.
                    rapidjson::Value valueObj(rapidjson::kObjectType);
                    doc.AddMember("value", valueObj, allocator);
                }
            }

            return documentToString(doc);
        },
        response);
}

std::string ResponseSerializerJson::documentToString(const rapidjson::Document& doc)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
}

} // namespace DirtSim
