#include "RuleBasedBrain.h"
#include "core/MaterialType.h"
#include <algorithm>
#include <random>
#include <spdlog/spdlog.h>
#include <vector>

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
                trunk_base_ =
                    sensory.seed_position; // Lock trunk position to original seed location.
                spdlog::info(
                    "RuleBasedBrain: Growing first WOOD above seed at ({}, {}), trunk_base=({}, "
                    "{})",
                    wood_pos.x,
                    wood_pos.y,
                    trunk_base_.x,
                    trunk_base_.y);

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

    // Analyze tree structure for realistic growth.
    TreeMetrics metrics = analyzeTreeStructure(sensory);

    spdlog::info(
        "TreeMetrics: above={:.2f}, below={:.2f}, ratio={:.2f}, trunk_height={}, trunk_cells={}, "
        "branch_cells={}",
        metrics.above_ground_mass,
        metrics.below_ground_mass,
        metrics.below_ground_mass > 0 ? metrics.above_ground_mass / metrics.below_ground_mass
                                      : 999.0,
        metrics.trunk_height,
        metrics.trunk_cells.size(),
        metrics.branch_cells.size());

    // Priority 1: Ensure roots support canopy (above_ground <= 2 × below_ground).
    if (metrics.above_ground_mass > 1.0 * metrics.below_ground_mass) {
        Vector2i pos = findGrowthPosition(sensory, MaterialType::ROOT);
        if (checkGrowthSuitability(sensory, pos, MaterialType::ROOT)
            == GrowthSuitability::SUITABLE) {
            spdlog::info(
                "RuleBasedBrain: Growing ROOT for support (above={:.2f} > 2×below={:.2f})",
                metrics.above_ground_mass,
                metrics.below_ground_mass);
            return GrowRootCommand{ .target_pos = pos, .execution_time_seconds = 2.0 };
        }
        else {
            spdlog::warn(
                "RuleBasedBrain: Cannot find root location! (above={:.2f} > 2×below={:.2f})",
                metrics.above_ground_mass,
                metrics.below_ground_mass);
        }
    }

    // Priority 2: Build minimum trunk height before branching.
    if (metrics.trunk_height < 2) {
        Vector2i pos = findTrunkGrowthPosition(sensory, metrics);
        if (checkGrowthSuitability(sensory, pos, MaterialType::WOOD)
            == GrowthSuitability::SUITABLE) {
            spdlog::info("RuleBasedBrain: Growing TRUNK upward (height={})", metrics.trunk_height);
            return GrowWoodCommand{ .target_pos = pos, .execution_time_seconds = 3.0 };
        }
    }

    // Priority 3: Balance left/right growth with branches.
    spdlog::info(
        "RuleBasedBrain: trunk_height={}, left={:.2f}, right={:.2f}",
        metrics.trunk_height,
        metrics.left_mass,
        metrics.right_mass);

    if (shouldStartBranch(metrics)) {
        double imbalance_ratio = metrics.left_mass > 0 && metrics.right_mass > 0
            ? std::min(metrics.left_mass, metrics.right_mass)
                / std::max(metrics.left_mass, metrics.right_mass)
            : 0.0;

        // Force branch growth if imbalanced OR alternating with trunk.
        if (imbalance_ratio < 0.8 || metrics.trunk_height % 2 == 0) {
            Vector2i pos = findBranchGrowthPosition(sensory, metrics);
            if (checkGrowthSuitability(sensory, pos, MaterialType::WOOD)
                == GrowthSuitability::SUITABLE) {
                spdlog::info(
                    "RuleBasedBrain: Growing BRANCH (left_mass={:.2f}, right_mass={:.2f}, "
                    "imbalance={:.2f})",
                    metrics.left_mass,
                    metrics.right_mass,
                    imbalance_ratio);
                return GrowWoodCommand{ .target_pos = pos, .execution_time_seconds = 3.0 };
            }
        }
    }

    // Priority 4: Grow leaves at branch tips.
    TreeComposition comp = analyzeTreeComposition(sensory);
    double total = comp.total_cells > 0 ? comp.total_cells : 1.0;
    double leaf_ratio = comp.leaf_count / total;

    if (leaf_ratio < 0.25) {
        Vector2i pos = findLeafGrowthPositionOnBranches(sensory, metrics);
        if (checkGrowthSuitability(sensory, pos, MaterialType::LEAF)
            == GrowthSuitability::SUITABLE) {
            spdlog::info("RuleBasedBrain: Growing LEAF at tips");
            return GrowLeafCommand{ .target_pos = pos, .execution_time_seconds = 0.5 };
        }
    }

    // Priority 5: Continue trunk/branch growth.
    Vector2i pos = findTrunkGrowthPosition(sensory, metrics);
    if (checkGrowthSuitability(sensory, pos, MaterialType::WOOD) == GrowthSuitability::SUITABLE) {
        return GrowWoodCommand{ .target_pos = pos, .execution_time_seconds = 3.0 };
    }

    pos = findBranchGrowthPosition(sensory, metrics);
    if (checkGrowthSuitability(sensory, pos, MaterialType::WOOD) == GrowthSuitability::SUITABLE) {
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

    // Scan all cells and categorize them.
    for (int y = 0; y < sensory.GRID_SIZE; y++) {
        for (int x = 0; x < sensory.GRID_SIZE; x++) {
            Vector2i world_pos = sensory.world_offset + Vector2i{ x, y };
            const auto& hist = sensory.material_histograms[y][x];

            // Calculate cell mass.
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
            if (world_pos.y < seed.y)
                metrics.above_ground_mass += cell_mass;
            else if (world_pos.y > seed.y)
                metrics.below_ground_mass += cell_mass;

            // Identify trunk cells (vertical WOOD directly above seed).
            if (hist[wood_idx] > 0.5 && world_pos.x == seed.x && world_pos.y < seed.y) {
                metrics.trunk_cells.push_back(world_pos);
                spdlog::debug("Found trunk cell at ({}, {})", world_pos.x, world_pos.y);
            }

            // Identify branch cells (lateral WOOD, not on trunk).
            if (hist[wood_idx] > 0.5 && world_pos.x != seed.x && world_pos.y < seed.y) {
                metrics.branch_cells.push_back(world_pos);
            }
        }
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

        spdlog::info(
            "Trunk cells found: {} cells, seed at ({}, {})",
            metrics.trunk_cells.size(),
            seed.x,
            seed.y);
        for (const auto& tc : metrics.trunk_cells) {
            spdlog::info("  Trunk cell: ({}, {})", tc.x, tc.y);
        }

        // Count continuous cells from seed upward.
        int expected_y = seed.y - 1;
        spdlog::info("Starting trunk height count from expected_y={}", expected_y);
        for (const auto& trunk_cell : metrics.trunk_cells) {
            if (trunk_cell.y == expected_y) {
                metrics.trunk_height++;
                expected_y--;
                spdlog::info(
                    "  ✓ Matched! trunk_height now {}, next expected_y={}",
                    metrics.trunk_height,
                    expected_y);
            }
            else {
                spdlog::info(
                    "  ✗ Gap found: cell at y={}, expected y={}", trunk_cell.y, expected_y);
                break; // Gap in trunk.
            }
        }
    }

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

} // namespace DirtSim
