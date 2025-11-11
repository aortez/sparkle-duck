#include "RuleBasedBrain.h"
#include <spdlog/spdlog.h>

namespace DirtSim {

TreeCommand RuleBasedBrain::decide(const TreeSensoryData& sensory)
{
    // SEED stage: Wait until age 100, then germinate.
    if (sensory.stage == GrowthStage::SEED) {
        if (sensory.age >= 100 && !has_germinated_) {
            // Germinate: Convert SEED to WOOD at seed position.
            // For single-cell trees, world_offset is the seed position.
            has_germinated_ = true;
            spdlog::info(
                "RuleBasedBrain: Germinating SEED at ({}, {}) after {} timesteps",
                sensory.world_offset.x,
                sensory.world_offset.y,
                sensory.age);

            return GrowWoodCommand{ .target_pos = sensory.world_offset,
                                    .execution_time = 10,
                                    .energy_cost = 0.0 }; // Free for initial germination.
        }
        else {
            // Still waiting for germination conditions.
            return WaitCommand{ .duration = 10 };
        }
    }

    // GERMINATION stage: Grow first ROOT downward.
    if (sensory.stage == GrowthStage::GERMINATION) {
        if (!has_grown_first_root_) {
            // Grow root one cell below the main stem.
            Vector2i root_pos{ sensory.world_offset.x, sensory.world_offset.y + 1 };

            has_grown_first_root_ = true;
            spdlog::info("RuleBasedBrain: Growing first ROOT at ({}, {})", root_pos.x, root_pos.y);

            return GrowRootCommand{ .target_pos = root_pos,
                                    .execution_time = 20,
                                    .energy_cost = 0.0 }; // Free for initial growth.
        }
        else {
            // Germination complete, just wait (future: transition to SAPLING).
            return WaitCommand{ .duration = 100 };
        }
    }

    // SAPLING, MATURE, DECLINE stages: Not implemented yet.
    // For now, just wait.
    return WaitCommand{ .duration = 100 };
}

} // namespace DirtSim
