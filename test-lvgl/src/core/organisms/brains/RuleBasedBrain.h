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

// Canopy sector for radial balance analysis.
struct CanopySector {
    double mass = 0.0;
    int cell_count = 0;
};

// Tree structure metrics, re-derived from sensory data each frame.
// No cached state - always reflects current physical positions.
struct TreeMetrics {
    double left_mass = 0.0;
    double right_mass = 0.0;
    double above_ground_mass = 0.0;
    double below_ground_mass = 0.0;
    int trunk_height = 0; // Continuous vertical WOOD above seed.
    std::vector<Vector2i> trunk_cells;
    std::vector<Vector2i> branch_cells;

    // Branch tiers: y-offsets relative to seed where branches exist.
    // Used for 3-cell spacing rule. Seed counts as tier 0.
    std::vector<int> branch_tiers_relative; // Negative = above seed.

    // Canopy sectors (left/right × high/mid/low) for radial balance.
    CanopySector left_high, left_mid, left_low;
    CanopySector right_high, right_mid, right_low;

    // Center of mass (relative to seed position).
    Vector2d center_of_mass{ 0.0, 0.0 };
    double canopy_width = 0.0;
    double canopy_height = 0.0;

    // Methods implemented in .cpp.
    bool isTooFlat(double threshold = 1.5) const;
    bool canFitBranchAt(int relative_y) const;
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
    std::mt19937 rng_; // Per-brain RNG for deterministic growth.

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

    // Canopy balance and branch sizing.
    const CanopySector& findEmptiestSector(const TreeMetrics& metrics);
    int getBranchTargetLength(int branch_relative_y, int trunk_height);
};

} // namespace DirtSim
