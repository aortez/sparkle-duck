#pragma once

#include "CellGet.h"
#include "CellSet.h"
#include "Exit.h"
#include "GravitySet.h"
#include "Reset.h"
#include "StateGet.h"
#include "StepN.h"
#include <variant>

namespace DirtSim {

/**
 * @brief Variant containing all API command types.
 */
using ApiCommand = std::variant<
    Api::CellGet::Command,
    Api::CellSet::Command,
    Api::Exit::Command,
    Api::GravitySet::Command,
    Api::Reset::Command,
    Api::StateGet::Command,
    Api::StepN::Command
>;

} // namespace DirtSim
