#pragma once

#include "Cell.h"
#include "CommandWithCallback.h"
#include "MaterialType.h"
#include "Result.h"
#include "World.h"
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
 * @brief Get specific cell state.
 */
namespace CellGet {
struct Command {
    int x;
    int y;
        };

        struct Okay {
            Cell cell;
        };
        using Response = Result<Okay, ApiError>;
        using Cwc = CommandWithCallback<Command, Response>;
        } // namespace CellGet

    /**
     * @brief Set material in a cell.
     */
        namespace CellSet {
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
        struct Command {
            // No parameters needed.
        };

        using Response = Result<std::monostate, ApiError>;
        using Cwc = CommandWithCallback<Command, Response>;
        } // namespace Reset

        /**
         * @brief Get complete world state.
         */
        namespace StateGet {
        struct Command {
            // No parameters needed.
        };

        struct Okay {
            World world;
        };
        using Response = Result<Okay, ApiError>;
        using Cwc = CommandWithCallback<Command, Response>;
        } // namespace StateGet

    /**
     * @brief Advance simulation by N frames.
     */
        namespace StepN {
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
