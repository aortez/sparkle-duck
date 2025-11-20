#pragma once

#include "core/MaterialType.h"
#include "core/organisms/TreeBrain.h"

namespace DirtSim {

enum class GrowthSuitability { SUITABLE, BLOCKED, OUT_OF_BOUNDS };

struct TreeComposition {
    int root_count;
    int wood_count;
    int leaf_count;
    int total_cells;
};

class RuleBasedBrain : public TreeBrain {
public:
    RuleBasedBrain() = default;

    TreeCommand decide(const TreeSensoryData& sensory) override;

private:
    bool has_contacted_dirt_ = false;
    double dirt_contact_age_seconds_ = 0.0;
    Vector2i root_target_pos_;
    bool has_grown_first_root_ = false;
    bool has_grown_first_wood_ = false;

    GrowthSuitability checkGrowthSuitability(
        const TreeSensoryData& sensory, Vector2i world_pos, MaterialType target_material);
    TreeComposition analyzeTreeComposition(const TreeSensoryData& sensory);
    Vector2i findGrowthPosition(const TreeSensoryData& sensory, MaterialType target_material);
    bool hasWaterAccess(const TreeSensoryData& sensory);
};

} // namespace DirtSim
