#pragma once

#include "CellGet.h"
#include "CellSet.h"
#include "DiagramGet.h"
#include "Exit.h"
#include "GravitySet.h"
#include "PerfStatsGet.h"
#include "Reset.h"
#include "ScenarioConfigSet.h"
#include "SeedAdd.h"
#include "SimRun.h"
#include "SpawnDirtBall.h"
#include "StateGet.h"
#include "StepN.h"
#include "TimerStatsGet.h"
#include <variant>

namespace DirtSim {

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
    Api::Reset::Command,
    Api::ScenarioConfigSet::Command,
    Api::SeedAdd::Command,
    Api::SimRun::Command,
    Api::SpawnDirtBall::Command,
    Api::StateGet::Command,
    Api::StepN::Command,
    Api::TimerStatsGet::Command>;

} // namespace DirtSim
