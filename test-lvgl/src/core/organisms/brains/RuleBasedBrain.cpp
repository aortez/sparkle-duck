#include "RuleBasedBrain.h"
#include "core/MaterialType.h"
#include <algorithm>
#include <random>
#include <set>
#include <spdlog/spdlog.h>
#include <vector>

namespace DirtSim {

// Each ROOT cell in contact with dirt can support this many above-ground cells.
static constexpr int CELLS_PER_ROOT = 3;

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
            GrowthSuitability suitability =
                checkGrowthSuitability(sensory, root_target_pos_, MaterialType::ROOT);

            if (suitability == GrowthSuitability::SUITABLE) {
                spdlog::info(
                    "RuleBasedBrain: Observed DIRT for {} seconds, growing ROOT at ({}, {})",
                    observation_time,
                    root_target_pos_.x,
                    root_target_pos_.y);

                return GrowRootCommand{ .target_pos = root_target_pos_,
                                        .execution_time_seconds = 2.0 };
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

            GrowthSuitability suitability =
                checkGrowthSuitability(sensory, wood_pos, MaterialType::WOOD);

            if (suitability == GrowthSuitability::SUITABLE) {
                has_grown_first_wood_ = true;
                spdlog::info(
                    "RuleBasedBrain: Growing first WOOD above seed at ({}, {})",
                    wood_pos.x,
                    wood_pos.y);

                return GrowWoodCommand{ .target_pos = wood_pos, .execution_time_seconds = 3.0 };
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

    // Analyze tree structure for realistic growth (re-derived each frame).
    TreeMetrics metrics = analyzeTreeStructure(sensory);
    TreeComposition comp = analyzeTreeComposition(sensory);

    // Above-ground cells: WOOD + LEAF + SEED (the +1 accounts for seed).
    int above_ground_cells = comp.wood_count + comp.leaf_count + 1;
    int root_capacity = comp.root_count * CELLS_PER_ROOT;

    spdlog::debug(
        "TreeMetrics: above_cells={}, root_capacity={} ({}x{}), trunk_height={}, canopy={}x{}, "
        "flat={}",
        above_ground_cells,
        root_capacity,
        comp.root_count,
        CELLS_PER_ROOT,
        metrics.trunk_height,
        metrics.canopy_width,
        metrics.canopy_height,
        metrics.isTooFlat());

    // Priority 1: Ensure roots support canopy (cell count based).
    if (above_ground_cells > root_capacity) {
        Vector2i pos = findGrowthPosition(sensory, MaterialType::ROOT);
        if (checkGrowthSuitability(sensory, pos, MaterialType::ROOT)
            == GrowthSuitability::SUITABLE) {
            spdlog::debug(
                "RuleBasedBrain: [P1] Growing ROOT for support at ({},{}) - need {} more capacity",
                pos.x,
                pos.y,
                above_ground_cells - root_capacity);
            return GrowRootCommand{ .target_pos = pos, .execution_time_seconds = 2.0 };
        }
    }

    // Priority 2: Grow trunk if tree is too flat or trunk is too short.
    bool need_trunk = metrics.trunk_height < 3 || metrics.isTooFlat();
    if (need_trunk) {
        Vector2i pos = findTrunkGrowthPosition(sensory, metrics);
        if (checkGrowthSuitability(sensory, pos, MaterialType::WOOD)
            == GrowthSuitability::SUITABLE) {
            spdlog::debug(
                "RuleBasedBrain: [P2] Growing TRUNK at ({},{}) (height={}, flat={})",
                pos.x,
                pos.y,
                metrics.trunk_height,
                metrics.isTooFlat());
            return GrowWoodCommand{ .target_pos = pos, .execution_time_seconds = 3.0 };
        }
    }

    // Priority 3: Start new branch tier if spacing allows.
    // Find highest trunk cell where we can start a new branch (respecting 3-cell spacing).
    if (metrics.trunk_height >= 3) {
        for (const auto& trunk_cell : metrics.trunk_cells) {
            int relative_y = trunk_cell.y - sensory.seed_position.y;

            if (metrics.canFitBranchAt(relative_y)) {
                // Check target branch length for this tier.
                int target_length = getBranchTargetLength(relative_y, metrics.trunk_height);

                // Find how long existing branches are at this tier.
                int current_length = 0;
                for (const auto& branch : metrics.branch_cells) {
                    if (branch.y == trunk_cell.y) {
                        int dist = std::abs(branch.x - sensory.seed_position.x);
                        current_length = std::max(current_length, dist);
                    }
                }

                if (current_length < target_length) {
                    // Grow toward emptiest side.
                    bool prefer_left = metrics.left_mass < metrics.right_mass;
                    Vector2i left = trunk_cell + Vector2i{ -1, 0 };
                    Vector2i right = trunk_cell + Vector2i{ 1, 0 };
                    Vector2i pos = prefer_left ? left : right;

                    if (checkGrowthSuitability(sensory, pos, MaterialType::WOOD)
                        != GrowthSuitability::SUITABLE) {
                        pos = prefer_left ? right : left; // Try other side.
                    }

                    if (checkGrowthSuitability(sensory, pos, MaterialType::WOOD)
                        == GrowthSuitability::SUITABLE) {
                        spdlog::debug(
                            "RuleBasedBrain: [P3] Starting BRANCH at ({},{}) tier={}",
                            pos.x,
                            pos.y,
                            relative_y);
                        return GrowWoodCommand{ .target_pos = pos, .execution_time_seconds = 3.0 };
                    }
                }
            }
        }
    }

    // Priority 4: Extend existing branches toward emptiest canopy sector.
    const CanopySector& emptiest = findEmptiestSector(metrics);
    bool target_left =
        (&emptiest == &metrics.left_high || &emptiest == &metrics.left_mid
         || &emptiest == &metrics.left_low);

    for (const auto& branch : metrics.branch_cells) {
        int relative_y = branch.y - sensory.seed_position.y;
        int target_length = getBranchTargetLength(relative_y, metrics.trunk_height);
        int current_dist = std::abs(branch.x - sensory.seed_position.x);

        if (current_dist < target_length) {
            // Extend in the direction away from trunk.
            int direction = (branch.x < sensory.seed_position.x) ? -1 : 1;

            // If this branch is on the side we want to fill, prioritize it.
            bool is_target_side = (direction < 0) == target_left;
            if (!is_target_side && metrics.branch_cells.size() > 1) {
                continue; // Skip, check other branches first.
            }

            Vector2i pos = branch + Vector2i{ direction, 0 };
            if (checkGrowthSuitability(sensory, pos, MaterialType::WOOD)
                == GrowthSuitability::SUITABLE) {
                spdlog::debug(
                    "RuleBasedBrain: [P4] Extending BRANCH at ({},{}) toward {} sector",
                    pos.x,
                    pos.y,
                    target_left ? "left" : "right");
                return GrowWoodCommand{ .target_pos = pos, .execution_time_seconds = 3.0 };
            }
        }
    }

    // Priority 5: Grow leaves at branch tips.
    double total = comp.total_cells > 0 ? comp.total_cells : 1.0;
    double leaf_ratio = comp.leaf_count / total;
    if (leaf_ratio < 0.25 && metrics.branch_cells.size() > 0) {
        Vector2i pos = findLeafGrowthPositionOnBranches(sensory, metrics);
        if (checkGrowthSuitability(sensory, pos, MaterialType::LEAF)
            == GrowthSuitability::SUITABLE) {
            spdlog::debug("RuleBasedBrain: [P5] Growing LEAF at ({},{})", pos.x, pos.y);
            return GrowLeafCommand{ .target_pos = pos, .execution_time_seconds = 0.5 };
        }
    }

    // Priority 6: Continue trunk growth if nothing else to do.
    Vector2i pos = findTrunkGrowthPosition(sensory, metrics);
    if (checkGrowthSuitability(sensory, pos, MaterialType::WOOD) == GrowthSuitability::SUITABLE) {
        spdlog::debug("RuleBasedBrain: [P6] Fallback TRUNK growth at ({},{})", pos.x, pos.y);
        return GrowWoodCommand{ .target_pos = pos, .execution_time_seconds = 3.0 };
    }

    return WaitCommand{ .duration_seconds = 2.0 };
}

GrowthSuitability RuleBasedBrain::checkGrowthSuitability(
    const TreeSensoryData& sensory, Vector2i world_pos, MaterialType target_material)
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

    if (target_material == MaterialType::LEAF) {
        return (air > 0.5) ? GrowthSuitability::SUITABLE : GrowthSuitability::BLOCKED;
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

    if (target_material == MaterialType::LEAF) {
        int wood_idx = static_cast<int>(MaterialType::WOOD);
        Vector2i best_pos = seed;
        double best_distance = -1.0;

        for (int y = 0; y < sensory.GRID_SIZE; y++) {
            for (int x = 0; x < sensory.GRID_SIZE; x++) {
                if (sensory.material_histograms[y][x][wood_idx] > 0.5) {
                    Vector2i wood_pos = sensory.world_offset + Vector2i{ x, y };

                    Vector2i directions[] = { { 0, 1 }, { 0, -1 }, { -1, 0 }, { 1, 0 } };
                    for (const auto& dir : directions) {
                        Vector2i candidate = wood_pos + dir;

                        if (checkGrowthSuitability(sensory, candidate, MaterialType::LEAF)
                            == GrowthSuitability::SUITABLE) {
                            int dx = candidate.x - seed.x;
                            int dy = candidate.y - seed.y;
                            double dist = dx * dx + dy * dy;

                            if (dist > best_distance) {
                                best_distance = dist;
                                best_pos = candidate;
                            }
                        }
                    }
                }
            }
        }

        return best_pos;
    }

    if (target_material == MaterialType::ROOT) {
        // Find all ROOT cells and grow downward from the deepest ones.
        int root_idx = static_cast<int>(MaterialType::ROOT);
        Vector2i best_pos = seed;
        int best_depth = -1; // Higher y = deeper.

        for (int y = 0; y < sensory.GRID_SIZE; y++) {
            for (int x = 0; x < sensory.GRID_SIZE; x++) {
                if (sensory.material_histograms[y][x][root_idx] > 0.5) {
                    Vector2i root_pos = sensory.world_offset + Vector2i{ x, y };

                    // Prioritize downward growth: down, then lateral (cardinal only).
                    Vector2i directions[] = { { 0, 1 }, { -1, 0 }, { 1, 0 } };
                    for (const auto& dir : directions) {
                        Vector2i candidate = root_pos + dir;

                        if (checkGrowthSuitability(sensory, candidate, MaterialType::ROOT)
                            == GrowthSuitability::SUITABLE) {
                            // Prefer deeper positions (higher y values).
                            if (candidate.y > best_depth) {
                                best_depth = candidate.y;
                                best_pos = candidate;
                            }
                        }
                    }
                }
            }
        }

        return best_pos;
    }
    else if (target_material == MaterialType::WOOD) {
        // Find all WOOD cells and grow upward from the highest ones.
        int wood_idx = static_cast<int>(MaterialType::WOOD);
        Vector2i best_pos = seed;
        int best_height = INT32_MAX; // Lower y = higher.

        for (int y = 0; y < sensory.GRID_SIZE; y++) {
            for (int x = 0; x < sensory.GRID_SIZE; x++) {
                if (sensory.material_histograms[y][x][wood_idx] > 0.5) {
                    Vector2i wood_pos = sensory.world_offset + Vector2i{ x, y };

                    // Prioritize upward growth: up, then lateral (cardinal only).
                    Vector2i directions[] = { { 0, -1 }, { -1, 0 }, { 1, 0 } };
                    for (const auto& dir : directions) {
                        Vector2i candidate = wood_pos + dir;

                        if (checkGrowthSuitability(sensory, candidate, MaterialType::WOOD)
                            == GrowthSuitability::SUITABLE) {
                            // Prefer higher positions (lower y values).
                            if (candidate.y < best_height) {
                                best_height = candidate.y;
                                best_pos = candidate;
                            }
                        }
                    }
                }
            }
        }

        return best_pos;
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

TreeMetrics RuleBasedBrain::analyzeTreeStructure(const TreeSensoryData& sensory)
{
    TreeMetrics metrics;
    Vector2i seed = sensory.seed_position;

    int wood_idx = static_cast<int>(MaterialType::WOOD);
    int leaf_idx = static_cast<int>(MaterialType::LEAF);
    int root_idx = static_cast<int>(MaterialType::ROOT);
    int seed_idx = static_cast<int>(MaterialType::SEED);

    // Track bounds for canopy dimensions.
    int min_x = INT32_MAX, max_x = INT32_MIN;
    int min_y = INT32_MAX, max_y = INT32_MIN;
    double total_mass = 0.0;
    double com_x = 0.0, com_y = 0.0;

    // Track unique branch tiers (y-positions where branches exist).
    std::set<int> branch_tier_set;

    // Scan all cells and categorize them.
    for (int y = 0; y < sensory.GRID_SIZE; y++) {
        for (int x = 0; x < sensory.GRID_SIZE; x++) {
            Vector2i world_pos = sensory.world_offset + Vector2i{ x, y };
            const auto& hist = sensory.material_histograms[y][x];

            // Calculate cell mass (only above-ground tree materials for canopy).
            double wood_mass = (hist[wood_idx] > 0.5) ? 0.3 : 0.0;
            double leaf_mass = (hist[leaf_idx] > 0.5) ? 0.3 : 0.0;
            double root_mass = (hist[root_idx] > 0.5) ? 1.2 : 0.0;
            double seed_mass = (hist[seed_idx] > 0.5) ? 1.5 : 0.0;
            double cell_mass = wood_mass + leaf_mass + root_mass + seed_mass;

            if (cell_mass == 0.0) continue;

            // Left/right balance (all cells).
            if (world_pos.x < seed.x)
                metrics.left_mass += cell_mass;
            else if (world_pos.x > seed.x)
                metrics.right_mass += cell_mass;

            // Above/below ground (relative to seed y).
            int relative_y = world_pos.y - seed.y;
            if (relative_y < 0) {
                metrics.above_ground_mass += cell_mass;

                // Track canopy bounds (above-ground only).
                min_x = std::min(min_x, world_pos.x);
                max_x = std::max(max_x, world_pos.x);
                min_y = std::min(min_y, world_pos.y);
                max_y = std::max(max_y, world_pos.y);

                // Accumulate for center of mass.
                com_x += world_pos.x * cell_mass;
                com_y += world_pos.y * cell_mass;
                total_mass += cell_mass;
            }
            else if (relative_y > 0) {
                metrics.below_ground_mass += cell_mass;
            }

            // Identify trunk cells (vertical WOOD directly above seed).
            if (hist[wood_idx] > 0.5 && world_pos.x == seed.x && world_pos.y < seed.y) {
                metrics.trunk_cells.push_back(world_pos);
            }

            // Identify branch cells (lateral WOOD, not on trunk).
            if (hist[wood_idx] > 0.5 && world_pos.x != seed.x && world_pos.y < seed.y) {
                metrics.branch_cells.push_back(world_pos);
                branch_tier_set.insert(relative_y); // Track unique branch tiers.
            }

            // Assign to canopy sectors (above-ground, left/right × high/mid/low).
            if (relative_y < 0 && (wood_mass > 0.0 || leaf_mass > 0.0)) {
                bool is_left = world_pos.x < seed.x;
                bool is_right = world_pos.x > seed.x;

                // Determine height band (relative to trunk height, estimated).
                // For now, use fixed bands: high=-6+, mid=-3 to -5, low=-1 to -2.
                CanopySector* sector = nullptr;
                if (relative_y <= -6) {
                    sector =
                        is_left ? &metrics.left_high : (is_right ? &metrics.right_high : nullptr);
                }
                else if (relative_y <= -3) {
                    sector =
                        is_left ? &metrics.left_mid : (is_right ? &metrics.right_mid : nullptr);
                }
                else {
                    sector =
                        is_left ? &metrics.left_low : (is_right ? &metrics.right_low : nullptr);
                }

                if (sector) {
                    sector->mass += cell_mass;
                    sector->cell_count++;
                }
            }
        }
    }

    // Convert branch tier set to vector.
    metrics.branch_tiers_relative.assign(branch_tier_set.begin(), branch_tier_set.end());

    // Calculate center of mass (relative to seed).
    if (total_mass > 0.0) {
        metrics.center_of_mass.x = (com_x / total_mass) - seed.x;
        metrics.center_of_mass.y = (com_y / total_mass) - seed.y;
    }

    // Calculate canopy dimensions.
    if (min_x <= max_x && min_y <= max_y) {
        metrics.canopy_width = static_cast<double>(max_x - min_x + 1);
        metrics.canopy_height = static_cast<double>(seed.y - min_y); // Distance from seed to top.
    }

    // Calculate trunk height (continuous vertical WOOD).
    if (!metrics.trunk_cells.empty()) {
        // Sort by y (descending = bottom to top, starting from seed).
        std::sort(
            metrics.trunk_cells.begin(),
            metrics.trunk_cells.end(),
            [](const Vector2i& a, const Vector2i& b) {
                return a.y > b.y; // Higher y first (closer to seed).
            });

        // Count continuous cells from seed upward.
        int expected_y = seed.y - 1;
        for (const auto& trunk_cell : metrics.trunk_cells) {
            if (trunk_cell.y == expected_y) {
                metrics.trunk_height++;
                expected_y--;
            }
            else {
                break; // Gap in trunk.
            }
        }
    }

    spdlog::debug(
        "TreeMetrics: trunk_height={}, canopy={}x{}, COM=({:.1f},{:.1f}), branch_tiers={}",
        metrics.trunk_height,
        metrics.canopy_width,
        metrics.canopy_height,
        metrics.center_of_mass.x,
        metrics.center_of_mass.y,
        metrics.branch_tiers_relative.size());

    return metrics;
}

bool RuleBasedBrain::shouldStartBranch(const TreeMetrics& metrics)
{
    // Start a branch if:
    // 1. Trunk is at least 2 cells tall, OR
    // 2. There are a few trunk cells since last branch.
    return metrics.trunk_height >= 2;
}

Vector2i RuleBasedBrain::findTrunkGrowthPosition(
    const TreeSensoryData& sensory, const TreeMetrics& metrics)
{
    Vector2i seed = sensory.seed_position;

    // Trunk grows straight up from seed.
    Vector2i trunk_top = seed + Vector2i{ 0, -metrics.trunk_height - 1 };

    // Verify it's suitable.
    if (checkGrowthSuitability(sensory, trunk_top, MaterialType::WOOD)
        == GrowthSuitability::SUITABLE) {
        return trunk_top;
    }

    return seed; // Fallback.
}

Vector2i RuleBasedBrain::findBranchGrowthPosition(
    const TreeSensoryData& sensory, const TreeMetrics& metrics)
{
    Vector2i seed = sensory.seed_position;

    // Branches grow laterally from trunk cells.
    // Prefer growing in the deficient side (left vs right).
    bool prefer_left = metrics.left_mass < metrics.right_mass;

    std::vector<std::pair<Vector2i, double>> weighted_candidates;

    // Check all trunk cells for branch opportunities.
    for (const auto& trunk_pos : metrics.trunk_cells) {
        // Try left and right.
        Vector2i left = trunk_pos + Vector2i{ -1, 0 };
        Vector2i right = trunk_pos + Vector2i{ 1, 0 };

        if (checkGrowthSuitability(sensory, left, MaterialType::WOOD)
            == GrowthSuitability::SUITABLE) {
            double weight = prefer_left ? 10.0 : 1.0; // 10x weight for deficient side.
            weighted_candidates.push_back({ left, weight });
        }

        if (checkGrowthSuitability(sensory, right, MaterialType::WOOD)
            == GrowthSuitability::SUITABLE) {
            double weight = prefer_left ? 1.0 : 10.0;
            weighted_candidates.push_back({ right, weight });
        }
    }

    // Randomly pick weighted by side preference.
    if (weighted_candidates.empty()) {
        return seed;
    }

    double total_weight = 0.0;
    for (const auto& [pos, weight] : weighted_candidates) {
        total_weight += weight;
    }

    std::uniform_real_distribution<double> dist(0.0, total_weight);
    double random_value = dist(rng_);

    double cumulative = 0.0;
    for (const auto& [pos, weight] : weighted_candidates) {
        cumulative += weight;
        if (random_value <= cumulative) {
            return pos;
        }
    }

    return weighted_candidates.back().first;
}

Vector2i RuleBasedBrain::findLeafGrowthPositionOnBranches(
    const TreeSensoryData& sensory, const TreeMetrics& metrics)
{
    (void)metrics; // Future: could use branch_cells for more targeted growth.

    Vector2i seed = sensory.seed_position;
    int wood_idx = static_cast<int>(MaterialType::WOOD);

    // Find WOOD cells (trunk + branches), weighted by distance from seed.
    // Prefer top/bottom neighbors (not sides).
    std::vector<std::pair<Vector2i, double>> weighted_candidates;

    for (int y = 0; y < sensory.GRID_SIZE; y++) {
        for (int x = 0; x < sensory.GRID_SIZE; x++) {
            if (sensory.material_histograms[y][x][wood_idx] > 0.5) {
                Vector2i wood_pos = sensory.world_offset + Vector2i{ x, y };

                // Calculate distance from seed (favor tips).
                int dx = wood_pos.x - seed.x;
                int dy = wood_pos.y - seed.y;
                double distance = std::sqrt(dx * dx + dy * dy);

                // Check top/bottom neighbors (prefer these over left/right).
                Vector2i top = wood_pos + Vector2i{ 0, -1 };
                Vector2i bottom = wood_pos + Vector2i{ 0, 1 };

                if (checkGrowthSuitability(sensory, top, MaterialType::LEAF)
                    == GrowthSuitability::SUITABLE) {
                    double weight = (distance + 1.0) * 2.0; // Distance weight + top/bottom bonus.
                    weighted_candidates.push_back({ top, weight });
                }

                if (checkGrowthSuitability(sensory, bottom, MaterialType::LEAF)
                    == GrowthSuitability::SUITABLE) {
                    double weight = (distance + 1.0) * 2.0;
                    weighted_candidates.push_back({ bottom, weight });
                }

                // Also check sides but with lower weight.
                Vector2i left = wood_pos + Vector2i{ -1, 0 };
                Vector2i right = wood_pos + Vector2i{ 1, 0 };

                if (checkGrowthSuitability(sensory, left, MaterialType::LEAF)
                    == GrowthSuitability::SUITABLE) {
                    double weight = distance + 1.0; // No bonus for sides.
                    weighted_candidates.push_back({ left, weight });
                }

                if (checkGrowthSuitability(sensory, right, MaterialType::LEAF)
                    == GrowthSuitability::SUITABLE) {
                    double weight = distance + 1.0;
                    weighted_candidates.push_back({ right, weight });
                }
            }
        }
    }

    // Randomly pick weighted by distance (farther = more likely).
    if (weighted_candidates.empty()) {
        return seed;
    }

    double total_weight = 0.0;
    for (const auto& [pos, weight] : weighted_candidates) {
        total_weight += weight;
    }

    std::uniform_real_distribution<double> dist(0.0, total_weight);
    double random_value = dist(rng_);

    double cumulative = 0.0;
    for (const auto& [pos, weight] : weighted_candidates) {
        cumulative += weight;
        if (random_value <= cumulative) {
            return pos;
        }
    }

    return weighted_candidates.back().first;
}

// TreeMetrics method implementations.

bool TreeMetrics::isTooFlat(double threshold) const
{
    if (canopy_height < 1.0) return false; // No canopy yet.
    return (canopy_width / canopy_height) > threshold;
}

bool TreeMetrics::canFitBranchAt(int relative_y) const
{
    // Seed counts as tier 0, so new branch must be at least 3 cells above.
    if (relative_y > -3) return false;

    for (int tier : branch_tiers_relative) {
        if (std::abs(tier - relative_y) < 3) return false;
    }
    return true;
}

const CanopySector& RuleBasedBrain::findEmptiestSector(const TreeMetrics& metrics)
{
    // Find sector with least mass to prioritize growth there.
    const CanopySector* emptiest = &metrics.left_high;
    double min_mass = metrics.left_high.mass;

    auto check = [&](const CanopySector& sector) {
        if (sector.mass < min_mass) {
            min_mass = sector.mass;
            emptiest = &sector;
        }
    };

    check(metrics.left_mid);
    check(metrics.left_low);
    check(metrics.right_high);
    check(metrics.right_mid);
    check(metrics.right_low);

    return *emptiest;
}

int RuleBasedBrain::getBranchTargetLength(int branch_relative_y, int trunk_height)
{
    // Lower branches (closer to seed) should be longer.
    // Higher branches (further from seed) should be shorter.
    // This creates a conifer/Christmas tree shape.

    if (trunk_height <= 0) return 1;

    // branch_relative_y is negative (above seed).
    // -1 = just above seed (lowest branch), -trunk_height = top.
    double height_ratio = static_cast<double>(-branch_relative_y) / trunk_height;

    // Base length of 3, tapering to 1 at the top.
    int max_length = 3;
    int min_length = 1;
    int target = max_length - static_cast<int>((max_length - min_length) * height_ratio);

    return std::max(min_length, target);
}

} // namespace DirtSim
