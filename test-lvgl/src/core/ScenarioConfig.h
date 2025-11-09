#pragma once

#include "ReflectSerializer.h"
#include <nlohmann/json.hpp>
#include <string>
#include <variant>
#include <zpp_bits.h>

namespace DirtSim {

/**
 * @brief Configuration structs for each scenario type.
 *
 * These structs define the runtime-configurable parameters for each scenario.
 * They are automatically serialized via ReflectSerializer and transmitted
 * to the UI in WorldData for display/editing.
 */

/**
 * @brief Empty scenario - no configuration needed.
 */
struct EmptyConfig {
    using serialize = zpp::bits::members<0>;
};

/**
 * @brief Sandbox scenario - interactive playground with configurable features.
 */
struct SandboxConfig {
    using serialize = zpp::bits::members<5>;

    // Initial setup features.
    bool quadrant_enabled = true;          // Lower-right quadrant filled with dirt.

    // Continuous particle generation features.
    bool water_column_enabled = true;      // Water column on left side (5 wide × 20 tall).
    bool right_throw_enabled = true;       // Periodic dirt throw from right side.
    bool top_drop_enabled = true;          // Periodic dirt drop from top.
    double rain_rate = 0.0;                // Rain rate in drops per second (0 = disabled).
};

/**
 * @brief Dam break scenario - water behind barrier.
 */
struct DamBreakConfig {
    using serialize = zpp::bits::members<3>;

    double dam_height = 10.0;              // Height of dam wall.
    bool auto_release = false;             // Automatically break dam after delay.
    double release_time = 2.0;             // Time in seconds before auto-release.
};

/**
 * @brief Raining scenario - continuous rain.
 */
struct RainingConfig {
    using serialize = zpp::bits::members<2>;

    double rain_rate = 5.0;                // Rain rate in drops per second.
    bool puddle_floor = true;              // Add floor for puddles to form.
};

/**
 * @brief Water equalization scenario - pressure equilibration test.
 */
struct WaterEqualizationConfig {
    using serialize = zpp::bits::members<3>;

    double left_height = 15.0;             // Water column height on left.
    double right_height = 5.0;             // Water column height on right.
    bool separator_enabled = true;         // Start with separator wall.
};

/**
 * @brief Falling dirt scenario - gravity and pile formation.
 */
struct FallingDirtConfig {
    using serialize = zpp::bits::members<2>;

    double drop_height = 20.0;             // Height from which dirt drops.
    double drop_rate = 2.0;                // Drop rate in particles per second.
};

/**
 * @brief Variant type containing all scenario configurations.
 *
 * Use std::visit() or std::get<>() to access the active config.
 */
using ScenarioConfig = std::variant<
    EmptyConfig,
    SandboxConfig,
    DamBreakConfig,
    RainingConfig,
    WaterEqualizationConfig,
    FallingDirtConfig
>;

/**
 * @brief Get scenario ID string from config variant.
 * @param config The scenario config variant.
 * @return Scenario ID string (e.g., "sandbox", "dam_break").
 */
inline std::string getScenarioId(const ScenarioConfig& config)
{
    return std::visit([](auto&& c) -> std::string {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, EmptyConfig>) return "empty";
        else if constexpr (std::is_same_v<T, SandboxConfig>) return "sandbox";
        else if constexpr (std::is_same_v<T, DamBreakConfig>) return "dam_break";
        else if constexpr (std::is_same_v<T, RainingConfig>) return "raining";
        else if constexpr (std::is_same_v<T, WaterEqualizationConfig>) return "water_equalization";
        else if constexpr (std::is_same_v<T, FallingDirtConfig>) return "falling_dirt";
        else return "unknown";
    }, config);
}

} // namespace DirtSim

/**
 * ADL (Argument-Dependent Lookup) functions for nlohmann::json conversion of ScenarioConfig variant.
 */
namespace DirtSim {

inline void to_json(nlohmann::json& j, const ScenarioConfig& config)
{
    std::visit([&j](auto&& c) {
        using T = std::decay_t<decltype(c)>;

        // Use ReflectSerializer for automatic field serialization.
        j = ReflectSerializer::to_json(c);

        // Add type discriminator for variant deserialization.
        if constexpr (std::is_same_v<T, EmptyConfig>) {
            j["type"] = "empty";
        }
        else if constexpr (std::is_same_v<T, SandboxConfig>) {
            j["type"] = "sandbox";
        }
        else if constexpr (std::is_same_v<T, DamBreakConfig>) {
            j["type"] = "dam_break";
        }
        else if constexpr (std::is_same_v<T, RainingConfig>) {
            j["type"] = "raining";
        }
        else if constexpr (std::is_same_v<T, WaterEqualizationConfig>) {
            j["type"] = "water_equalization";
        }
        else if constexpr (std::is_same_v<T, FallingDirtConfig>) {
            j["type"] = "falling_dirt";
        }
    }, config);
}

inline void from_json(const nlohmann::json& j, ScenarioConfig& config)
{
    std::string type = j.value("type", "empty");

    // Use ReflectSerializer for automatic field deserialization.
    if (type == "empty") {
        config = ReflectSerializer::from_json<EmptyConfig>(j);
    }
    else if (type == "sandbox") {
        config = ReflectSerializer::from_json<SandboxConfig>(j);
    }
    else if (type == "dam_break") {
        config = ReflectSerializer::from_json<DamBreakConfig>(j);
    }
    else if (type == "raining") {
        config = ReflectSerializer::from_json<RainingConfig>(j);
    }
    else if (type == "water_equalization") {
        config = ReflectSerializer::from_json<WaterEqualizationConfig>(j);
    }
    else if (type == "falling_dirt") {
        config = ReflectSerializer::from_json<FallingDirtConfig>(j);
    }
    else {
        config = EmptyConfig{};
    }
}

} // namespace DirtSim

