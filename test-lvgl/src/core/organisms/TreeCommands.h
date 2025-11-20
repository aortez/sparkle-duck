#pragma once

#include "core/Vector2i.h"
#include <variant>

namespace DirtSim {

struct GrowWoodCommand {
    Vector2i target_pos;
    double execution_time_seconds = 3.0;
    double energy_cost = 10.0;
};

struct GrowLeafCommand {
    Vector2i target_pos;
    double execution_time_seconds = 0.5;
    double energy_cost = 8.0;
};

struct GrowRootCommand {
    Vector2i target_pos;
    double execution_time_seconds = 2.0;
    double energy_cost = 12.0;
};

struct ReinforceCellCommand {
    Vector2i position;
    double execution_time_seconds = 0.5;
    double energy_cost = 5.0;
};

struct ProduceSeedCommand {
    Vector2i position;
    double execution_time_seconds = 2.0;
    double energy_cost = 50.0;
};

struct WaitCommand {
    double duration_seconds = 0.2;
};

using TreeCommand = std::variant<
    GrowWoodCommand,
    GrowLeafCommand,
    GrowRootCommand,
    ReinforceCellCommand,
    ProduceSeedCommand,
    WaitCommand>;

} // namespace DirtSim
