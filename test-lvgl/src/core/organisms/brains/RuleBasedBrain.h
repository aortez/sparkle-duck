#pragma once

#include "core/organisms/TreeBrain.h"

namespace DirtSim {

/**
 * Simple rule-based brain for tree organisms.
 *
 * Implements basic growth patterns using hand-coded rules:
 * - SEED stage: Wait 100 timesteps, then germinate.
 * - GERMINATION: Convert SEED to WOOD, grow ROOT downward.
 * - SAPLING: Basic growth pattern (future).
 * - MATURE: Balanced growth (future).
 */
class RuleBasedBrain : public TreeBrain {
public:
    RuleBasedBrain() = default;

    /**
     * Decide next action based on growth stage and tree state.
     */
    TreeCommand decide(const TreeSensoryData& sensory) override;

private:
    // Brain state tracking.
    bool has_germinated_ = false;
    bool has_grown_first_root_ = false;
};

} // namespace DirtSim
