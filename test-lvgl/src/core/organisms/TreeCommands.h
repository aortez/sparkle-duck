#pragma once

#include "core/Vector2i.h"
#include <variant>

namespace DirtSim {

// Energy costs are determined by TreeCommandProcessor, not by command callers.

struct GrowWoodCommand {
    Vector2i target_pos;
    double execution_time_seconds = 3.0;
};

struct GrowLeafCommand {
    Vector2i target_pos;
    double execution_time_seconds = 0.5;
};

struct GrowRootCommand {
    Vector2i target_pos;
    double execution_time_seconds = 2.0;
};

struct ReinforceCellCommand {
    Vector2i position;
    double execution_time_seconds = 0.5;
};

struct ProduceSeedCommand {
    Vector2i position;
    double execution_time_seconds = 2.0;
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
