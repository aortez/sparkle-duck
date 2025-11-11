#pragma once

#include "TreeTypes.h"

namespace DirtSim {

/**
 * Abstract interface for tree brain implementations.
 *
 * Brains make growth decisions based on sensory data.
 * Different implementations can use:
 * - Hand-coded rules (RuleBasedBrain).
 * - Neural networks (NeuralNetBrain).
 * - LLM integration (LLMBrain).
 *
 * Brain state and memory are managed by concrete implementations.
 */
class TreeBrain {
public:
    virtual ~TreeBrain() = default;

    /**
     * Decide the next action based on sensory data.
     *
     * @param sensory Scale-invariant sensory data from the tree.
     * @return The next command to execute.
     */
    virtual TreeCommand decide(const TreeSensoryData& sensory) = 0;
};

} // namespace DirtSim
