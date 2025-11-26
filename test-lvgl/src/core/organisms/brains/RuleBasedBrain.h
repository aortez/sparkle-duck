#pragma once

#include "core/MaterialType.h"
#include "core/organisms/TreeBrain.h"
#include <random>

namespace DirtSim {

enum class GrowthSuitability { SUITABLE, BLOCKED, OUT_OF_BOUNDS };

struct TreeComposition {
    int root_count;
    int wood_count;
    int leaf_count;
    int total_cells;
};

struct TreeMetrics {
    double left_mass = 0.0;
    double right_mass = 0.0;
    double above_ground_mass = 0.0;
    double below_ground_mass = 0.0;
    int trunk_height = 0; // Continuous vertical WOOD above seed.
    std::vector<Vector2i> trunk_cells;
    std::vector<Vector2i> branch_cells;
};

class RuleBasedBrain : public TreeBrain {
public:
    RuleBasedBrain() : rng_(42) {} // Fixed seed for deterministic growth.

    TreeCommand decide(const TreeSensoryData& sensory) override;

    // Set RNG seed for testing different growth patterns.
    void setRandomSeed(uint32_t seed) { rng_.seed(seed); }

private:
    bool has_contacted_dirt_ = false;
    double dirt_contact_age_seconds_ = 0.0;
    Vector2i root_target_pos_;
    bool has_grown_first_root_ = false;
    bool has_grown_first_wood_ = false;
    Vector2i trunk_base_; // Original seed position for trunk tracking (doesn't move with physics).
    std::mt19937 rng_;    // Per-brain RNG for deterministic growth.

    GrowthSuitability checkGrowthSuitability(
        const TreeSensoryData& sensory, Vector2i world_pos, MaterialType target_material);
    TreeComposition analyzeTreeComposition(const TreeSensoryData& sensory);
    Vector2i findGrowthPosition(const TreeSensoryData& sensory, MaterialType target_material);
    bool hasWaterAccess(const TreeSensoryData& sensory);

    // Tree structure analysis and realistic growth.
    TreeMetrics analyzeTreeStructure(const TreeSensoryData& sensory);
    Vector2i findTrunkGrowthPosition(const TreeSensoryData& sensory, const TreeMetrics& metrics);
    Vector2i findBranchGrowthPosition(const TreeSensoryData& sensory, const TreeMetrics& metrics);
    Vector2i findLeafGrowthPositionOnBranches(
        const TreeSensoryData& sensory, const TreeMetrics& metrics);
    bool shouldStartBranch(const TreeMetrics& metrics);
};

} // namespace DirtSim
