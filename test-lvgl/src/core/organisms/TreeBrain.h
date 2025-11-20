#pragma once

#include "TreeCommands.h"
#include "TreeSensoryData.h"

namespace DirtSim {

class TreeBrain {
public:
    virtual ~TreeBrain() = default;

    virtual TreeCommand decide(const TreeSensoryData& sensory) = 0;
};

} // namespace DirtSim
