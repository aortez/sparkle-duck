#pragma once

#include "CommandWithCallback.h"
#include "MaterialType.h"
#include "Result.h"
#include "lvgl/src/libs/thorvg/rapidjson/document.h"
#include <cstdint>
#include <string>
#include <variant>

namespace DirtSim {

/**
 * @brief Error type for API command responses.
 */
struct ApiError {
    std::string message;

    ApiError() : message("Unknown error") {}
    explicit ApiError(const std::string& msg) : message(msg) {}
    ApiError(const char* msg) : message(msg) {}
};

namespace Api {

    /**
     * @brief Get specific cell state as JSON.
     */
    namespace CellGet {
        /// [[serialize]]
        struct Command {
            int x;
            int y;
        };

        struct Okay {
            rapidjson::Document cellJson;
        };
        using Response = Result<Okay, ApiError>;
        using Cwc = CommandWithCallback<Command, Response>;
    } // namespace CellGet

    /**
     * @brief Set material in a cell.
     */
    namespace CellSet {
        /// [[serialize]]
        struct Command {
            int x;
            int y;
            MaterialType material;
            double fill = 1.0;
        };

        using Response = Result<std::monostate, ApiError>;
        using Cwc = CommandWithCallback<Command, Response>;
    } // namespace CellSet

    /**
     * @brief Set gravity strength.
     */
    namespace GravitySet {
        /// [[serialize]]
        struct Command {
            double gravity;
        };

        using Response = Result<std::monostate, ApiError>;
        using Cwc = CommandWithCallback<Command, Response>;
    } // namespace GravitySet

    /**
     * @brief Reset simulation to initial state.
     */
    namespace Reset {
        /// [[serialize]]
        struct Command {
            // No parameters needed.
        };

        using Response = Result<std::monostate, ApiError>;
        using Cwc = CommandWithCallback<Command, Response>;
    } // namespace Reset

    /**
     * @brief Get complete world state as JSON.
     */
    namespace StateGet {
        /// [[serialize]]
        struct Command {
            // No parameters needed.
        };

        struct Okay {
            rapidjson::Document worldJson;
        };
        using Response = Result<Okay, ApiError>;
        using Cwc = CommandWithCallback<Command, Response>;
    } // namespace StateGet

    /**
     * @brief Advance simulation by N frames.
     */
    namespace StepN {
        /// [[serialize]]
        struct Command {
            int frames = 1;
        };

        struct Okay {
            uint32_t timestep;
        };
        using Response = Result<Okay, ApiError>;
        using Cwc = CommandWithCallback<Command, Response>;
    } // namespace StepN

} // namespace Api

/**
 * @brief Variant containing all API command types.
 * Used by CommandDeserializerJson as output type.
 */
using ApiCommand = std::variant<
    Api::CellGet::Command,
    Api::CellSet::Command,
    Api::GravitySet::Command,
    Api::Reset::Command,
    Api::StateGet::Command,
    Api::StepN::Command
>;

} // namespace DirtSim
