#pragma once

#include "../ApiCommands.h"
#include <string>
#include <variant>

namespace DirtSim {

/**
 * @brief Variant containing all API response types.
 */
using ApiResponse = std::variant<
    Api::CellGet::Response,
    Api::CellSet::Response,
    Api::GravitySet::Response,
    Api::Reset::Response,
    Api::StateGet::Response,
    Api::StepN::Response
>;

/**
 * @brief Serializes API response objects into JSON strings.
 *
 * Pure serialization - converts Response objects to JSON without
 * any side effects. Does not know about state machines, callbacks,
 * or network layers.
 */
class ResponseSerializerJson {
public:
    /**
     * @brief Serialize response variant into JSON string.
     * @param response The response to serialize.
     * @return JSON string representation.
     */
    std::string serialize(const ApiResponse& response);

    /**
     * @brief Helper to convert rapidjson::Document to JSON string.
     * @param doc The document to serialize.
     * @return JSON string.
     */
    static std::string documentToString(const rapidjson::Document& doc);
};

} // namespace DirtSim
