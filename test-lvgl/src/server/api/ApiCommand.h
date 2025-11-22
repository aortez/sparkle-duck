#pragma once

#include "ApiMacros.h"
#include "CellGet.h"
#include "CellSet.h"
#include "DiagramGet.h"
#include "Exit.h"
#include "GravitySet.h"
#include "PerfStatsGet.h"
#include "PhysicsSettingsGet.h"
#include "PhysicsSettingsSet.h"
#include "RenderFormatSet.h"
#include "Reset.h"
#include "ScenarioConfigSet.h"
#include "SeedAdd.h"
#include "SimRun.h"
#include "SpawnDirtBall.h"
#include "StateGet.h"
#include "StatusGet.h"
#include "TimerStatsGet.h"
#include "WorldResize.h"
#include <concepts>
#include <nlohmann/json.hpp>
#include <string_view>
#include <variant>

namespace DirtSim {

/**
 * @brief Concept for types that represent API commands.
 *
 * An API command type must provide:
 * - A static name() method returning the command name
 * - A toJson() method for serialization
 *
 * This concept enables type-safe generic programming with API commands
 * and provides clear compiler error messages when constraints are violated.
 */
template <typename T>
concept ApiCommandType = requires(T cmd) {
    { cmd.toJson() } -> std::convertible_to<nlohmann::json>;
    { T::name() } -> std::convertible_to<std::string_view>;
};

/**
 * @brief Variant containing all API command types.
 */
using ApiCommand = std::variant<
    Api::CellGet::Command,
    Api::CellSet::Command,
    Api::DiagramGet::Command,
    Api::Exit::Command,
    Api::GravitySet::Command,
    Api::PerfStatsGet::Command,
    Api::PhysicsSettingsGet::Command,
    Api::PhysicsSettingsSet::Command,
    Api::RenderFormatSet::Command,
    Api::Reset::Command,
    Api::ScenarioConfigSet::Command,
    Api::SeedAdd::Command,
    Api::SimRun::Command,
    Api::SpawnDirtBall::Command,
    Api::StateGet::Command,
    Api::StatusGet::Command,
    Api::TimerStatsGet::Command,
    Api::WorldResize::Command>;

} // namespace DirtSim
