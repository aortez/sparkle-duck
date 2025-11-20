#include "RuleBasedBrain.h"
#include "core/MaterialType.h"
#include <spdlog/spdlog.h>

namespace DirtSim {

TreeCommand RuleBasedBrain::decide(const TreeSensoryData& sensory)
{
    if (sensory.stage == GrowthStage::SEED) {
        if (!has_contacted_dirt_) {
            Vector2i seed = sensory.seed_position;
            Vector2i directions[] = { { 0, 1 },  { 0, -1 }, { -1, 0 },  { 1, 0 },
                                      { -1, 1 }, { 1, 1 },  { -1, -1 }, { 1, -1 } };

            for (const auto& dir : directions) {
                Vector2i check_pos = seed + dir;
                int mat_idx = static_cast<int>(MaterialType::DIRT);

                if (check_pos.x >= 0 && check_pos.y >= 0 && check_pos.x < sensory.GRID_SIZE
                    && check_pos.y < sensory.GRID_SIZE) {

                    int grid_x = check_pos.x - sensory.world_offset.x;
                    int grid_y = check_pos.y - sensory.world_offset.y;

                    if (grid_x >= 0 && grid_y >= 0 && grid_x < sensory.GRID_SIZE
                        && grid_y < sensory.GRID_SIZE) {

                        if (sensory.material_histograms[grid_y][grid_x][mat_idx] > 0.5) {
                            has_contacted_dirt_ = true;
                            dirt_contact_age_seconds_ = sensory.age_seconds;
                            root_target_pos_ = check_pos;

                            spdlog::info(
                                "RuleBasedBrain: Seed contacted DIRT at ({}, {}), observing for 2 "
                                "seconds",
                                check_pos.x,
                                check_pos.y);
                            break;
                        }
                    }
                }
            }

            return WaitCommand{ .duration_seconds = 0.2 };
        }

        double observation_time = sensory.age_seconds - dirt_contact_age_seconds_;
        if (observation_time >= 2.0) {
            GrowthSuitability suitability = checkGrowthSuitability(sensory, root_target_pos_);

            if (suitability == GrowthSuitability::SUITABLE) {
                spdlog::info(
                    "RuleBasedBrain: Observed DIRT for {} seconds, growing ROOT at ({}, {})",
                    observation_time,
                    root_target_pos_.x,
                    root_target_pos_.y);

                return GrowRootCommand{ .target_pos = root_target_pos_,
                                        .execution_time_seconds = 2.0,
                                        .energy_cost = 0.0 };
            }
            else {
                spdlog::warn(
                    "RuleBasedBrain: Cannot grow ROOT at ({}, {}) - blocked or out of bounds",
                    root_target_pos_.x,
                    root_target_pos_.y);
                return WaitCommand{ .duration_seconds = 1.0 };
            }
        }

        return WaitCommand{ .duration_seconds = 0.2 };
    }

    if (sensory.stage == GrowthStage::GERMINATION) {
        if (!has_grown_first_wood_) {
            Vector2i wood_pos{ sensory.seed_position.x, sensory.seed_position.y - 1 };

            GrowthSuitability suitability = checkGrowthSuitability(sensory, wood_pos);

            if (suitability == GrowthSuitability::SUITABLE) {
                has_grown_first_wood_ = true;
                spdlog::info(
                    "RuleBasedBrain: Growing first WOOD above seed at ({}, {})",
                    wood_pos.x,
                    wood_pos.y);

                return GrowWoodCommand{ .target_pos = wood_pos,
                                        .execution_time_seconds = 3.0,
                                        .energy_cost = 0.0 };
            }
            else {
                spdlog::warn(
                    "RuleBasedBrain: Cannot grow WOOD at ({}, {}) - blocked or out of bounds",
                    wood_pos.x,
                    wood_pos.y);
                return WaitCommand{ .duration_seconds = 1.0 };
            }
        }

        return WaitCommand{ .duration_seconds = 2.0 };
    }

    TreeComposition comp = analyzeTreeComposition(sensory);
    double total = comp.total_cells > 0 ? comp.total_cells : 1.0;
    double root_ratio = comp.root_count / total;
    double wood_ratio = comp.wood_count / total;
    double leaf_ratio = comp.leaf_count / total;

    bool has_water = hasWaterAccess(sensory);
    double root_target = has_water ? 0.15 : 0.30;

    if (root_ratio < root_target) {
        Vector2i pos = findGrowthPosition(sensory, MaterialType::ROOT);
        if (checkGrowthSuitability(sensory, pos) == GrowthSuitability::SUITABLE) {
            return GrowRootCommand{ .target_pos = pos,
                                    .execution_time_seconds = 2.0,
                                    .energy_cost = 12.0 };
        }
    }

    if (wood_ratio < 0.35) {
        Vector2i pos = findGrowthPosition(sensory, MaterialType::WOOD);
        if (checkGrowthSuitability(sensory, pos) == GrowthSuitability::SUITABLE) {
            return GrowWoodCommand{ .target_pos = pos,
                                    .execution_time_seconds = 3.0,
                                    .energy_cost = 10.0 };
        }
    }

    if (leaf_ratio < 0.25) {
        Vector2i pos = findGrowthPosition(sensory, MaterialType::LEAF);
        if (checkGrowthSuitability(sensory, pos) == GrowthSuitability::SUITABLE) {
            return GrowLeafCommand{ .target_pos = pos,
                                    .execution_time_seconds = 0.5,
                                    .energy_cost = 8.0 };
        }
    }

    Vector2i pos = findGrowthPosition(sensory, MaterialType::WOOD);
    if (checkGrowthSuitability(sensory, pos) == GrowthSuitability::SUITABLE) {
        return GrowWoodCommand{ .target_pos = pos,
                                .execution_time_seconds = 3.0,
                                .energy_cost = 10.0 };
    }

    return WaitCommand{ .duration_seconds = 2.0 };
}

GrowthSuitability RuleBasedBrain::checkGrowthSuitability(
    const TreeSensoryData& sensory, Vector2i world_pos)
{
    int grid_x = world_pos.x - sensory.world_offset.x;
    int grid_y = world_pos.y - sensory.world_offset.y;

    if (grid_x < 0 || grid_y < 0 || grid_x >= sensory.GRID_SIZE || grid_y >= sensory.GRID_SIZE) {
        return GrowthSuitability::OUT_OF_BOUNDS;
    }

    const auto& histogram = sensory.material_histograms[grid_y][grid_x];

    double air = histogram[static_cast<int>(MaterialType::AIR)];
    double dirt = histogram[static_cast<int>(MaterialType::DIRT)];
    double sand = histogram[static_cast<int>(MaterialType::SAND)];
    double water = histogram[static_cast<int>(MaterialType::WATER)];
    double wall = histogram[static_cast<int>(MaterialType::WALL)];
    double metal = histogram[static_cast<int>(MaterialType::METAL)];

    if (wall > 0.5 || metal > 0.5 || water > 0.5) {
        return GrowthSuitability::BLOCKED;
    }

    if (air > 0.3 || dirt > 0.3 || sand > 0.3) {
        return GrowthSuitability::SUITABLE;
    }

    return GrowthSuitability::BLOCKED;
}

TreeComposition RuleBasedBrain::analyzeTreeComposition(const TreeSensoryData& sensory)
{
    TreeComposition comp{ 0, 0, 0, 0 };

    int root_idx = static_cast<int>(MaterialType::ROOT);
    int wood_idx = static_cast<int>(MaterialType::WOOD);
    int leaf_idx = static_cast<int>(MaterialType::LEAF);

    for (int y = 0; y < sensory.GRID_SIZE; y++) {
        for (int x = 0; x < sensory.GRID_SIZE; x++) {
            const auto& hist = sensory.material_histograms[y][x];

            if (hist[root_idx] > 0.5) comp.root_count++;
            if (hist[wood_idx] > 0.5) comp.wood_count++;
            if (hist[leaf_idx] > 0.5) comp.leaf_count++;
        }
    }

    comp.total_cells = comp.root_count + comp.wood_count + comp.leaf_count;
    return comp;
}

Vector2i RuleBasedBrain::findGrowthPosition(
    const TreeSensoryData& sensory, MaterialType target_material)
{
    Vector2i seed = sensory.seed_position;

    if (target_material == MaterialType::ROOT) {
        Vector2i directions[] = { { 0, 1 }, { -1, 1 }, { 1, 1 }, { -1, 0 }, { 1, 0 } };
        for (const auto& dir : directions) {
            Vector2i pos = seed + dir;
            if (checkGrowthSuitability(sensory, pos) == GrowthSuitability::SUITABLE) {
                return pos;
            }
        }
    }
    else if (target_material == MaterialType::WOOD) {
        Vector2i directions[] = { { 0, -1 }, { -1, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 } };
        for (const auto& dir : directions) {
            Vector2i pos = seed + dir;
            if (checkGrowthSuitability(sensory, pos) == GrowthSuitability::SUITABLE) {
                return pos;
            }
        }
    }
    else if (target_material == MaterialType::LEAF) {
        Vector2i directions[] = { { -1, 0 }, { 1, 0 }, { -1, -1 }, { 1, -1 }, { 0, -1 } };
        for (const auto& dir : directions) {
            Vector2i pos = seed + dir;
            if (checkGrowthSuitability(sensory, pos) == GrowthSuitability::SUITABLE) {
                return pos;
            }
        }
    }

    return seed;
}

bool RuleBasedBrain::hasWaterAccess(const TreeSensoryData& sensory)
{
    int root_idx = static_cast<int>(MaterialType::ROOT);
    int water_idx = static_cast<int>(MaterialType::WATER);

    for (int y = 0; y < sensory.GRID_SIZE; y++) {
        for (int x = 0; x < sensory.GRID_SIZE; x++) {
            if (sensory.material_histograms[y][x][root_idx] > 0.5) {
                Vector2i directions[] = { { 0, 1 }, { 0, -1 }, { -1, 0 }, { 1, 0 } };
                for (const auto& dir : directions) {
                    int nx = x + dir.x;
                    int ny = y + dir.y;

                    if (nx >= 0 && ny >= 0 && nx < sensory.GRID_SIZE && ny < sensory.GRID_SIZE) {
                        if (sensory.material_histograms[ny][nx][water_idx] > 0.5) {
                            return true;
                        }
                    }
                }
            }
        }
    }

    return false;
}

} // namespace DirtSim
