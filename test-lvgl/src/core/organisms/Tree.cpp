#include "Tree.h"
#include "TreeCommandProcessor.h"
#include "core/Cell.h"
#include "core/MaterialType.h"
#include "core/World.h"
#include "core/WorldData.h"
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

namespace DirtSim {

double getBoneStiffness(MaterialType a, MaterialType b)
{
    // Order-independent lookup via sorting.
    if (a > b) std::swap(a, b);

    // Core structure - very stiff.
    if ((a == MaterialType::SEED && b == MaterialType::WOOD)
        || (a == MaterialType::SEED && b == MaterialType::ROOT)) {
        return 1.0;
    }

    // Trunk and branches.
    if (a == MaterialType::WOOD && b == MaterialType::WOOD) {
        return 0.8;
    }

    // Root system - somewhat flexible.
    if (a == MaterialType::ROOT && b == MaterialType::ROOT) {
        return 0.5;
    }
    if (a == MaterialType::ROOT && b == MaterialType::WOOD) {
        return 0.6;
    }

    // Foliage - stiff attachment to wood, flexible between leaves.
    if (a == MaterialType::LEAF && b == MaterialType::WOOD) {
        return 3.0; // Strong attachment to prevent leaves from falling.
    }
    if (a == MaterialType::LEAF && b == MaterialType::LEAF) {
        return 0.1;
    }

    // Default for any other organism material pairs.
    return 0.3;
}

void Tree::createBonesForCell(Vector2i new_cell, MaterialType material, const World& world)
{
    const WorldData& data = world.getData();
    int bones_created = 0;

    spdlog::info(
        "Tree {}: createBonesForCell for {} at ({},{})",
        id,
        getMaterialName(material),
        new_cell.x,
        new_cell.y);

    // Check cardinal (non-diagonal) neighbors for cells belonging to this organism.
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue; // Skip self.

            // Skip diagonal neighbors - only cardinal directions.
            if (dx != 0 && dy != 0) continue;

            int nx = new_cell.x + dx;
            int ny = new_cell.y + dy;

            if (nx < 0 || ny < 0 || static_cast<uint32_t>(nx) >= data.width
                || static_cast<uint32_t>(ny) >= data.height) {
                continue;
            }

            const Cell& neighbor = data.at(nx, ny);

            spdlog::info(
                "  Checking ({},{}) : mat={} org_id={} (my_id={}) fill={:.2f}",
                nx,
                ny,
                getMaterialName(neighbor.material_type),
                neighbor.organism_id,
                id,
                neighbor.fill_ratio);

            if (neighbor.organism_id != id) continue;

            Vector2i neighbor_pos{ nx, ny };
            double rest_dist = (dx == 0 || dy == 0) ? 1.0 : std::sqrt(2.0);
            double stiffness = getBoneStiffness(material, neighbor.material_type);

            // Determine hinge point and rotational damping.
            HingeEnd hinge = HingeEnd::NONE;
            double rot_damping = 0.0;

            // For leaf-wood connections, wood is the hinge (leaves swing around branches).
            if (material == MaterialType::LEAF && neighbor.material_type == MaterialType::WOOD) {
                hinge = HingeEnd::CELL_B; // Neighbor (wood) is the pivot.
                rot_damping = 1.0;        // Passive damping to prevent leaf swinging.
            }
            else if (
                material == MaterialType::WOOD && neighbor.material_type == MaterialType::LEAF) {
                hinge = HingeEnd::CELL_A; // New cell (wood) is the pivot.
                rot_damping = 1.0;        // Passive damping to prevent leaf swinging.
            }
            // Other bone types remain symmetric springs.

            bones.push_back(
                Bone{ new_cell, neighbor_pos, rest_dist, stiffness, hinge, rot_damping });
            bones_created++;

            spdlog::info(
                "Tree {}: Created bone ({},{}) <-> ({},{}) rest={:.2f} stiff={:.2f}",
                id,
                new_cell.x,
                new_cell.y,
                neighbor_pos.x,
                neighbor_pos.y,
                rest_dist,
                stiffness);
        }
    }

    if (bones_created == 0) {
        spdlog::warn(
            "Tree {}: NO BONES created for {} at ({},{}) - no adjacent organism cells found!",
            id,
            getMaterialName(material),
            new_cell.x,
            new_cell.y);
    }
}

Tree::Tree(TreeId id, std::unique_ptr<TreeBrain> brain) : id(id), brain_(std::move(brain))
{}

void Tree::update(World& world, double deltaTime)
{
    age_seconds += deltaTime;

    if (current_command.has_value()) {
        time_remaining_seconds -= deltaTime;
        if (time_remaining_seconds <= 0.0) {
            executeCommand(world);
            current_command.reset();
        }
    }

    if (!current_command.has_value()) {
        decideNextAction(world);
    }

    updateResources(world);
}

void Tree::executeCommand(World& world)
{
    CommandExecutionResult result = TreeCommandProcessor::execute(*this, world, *current_command);

    if (!result.succeeded()) {
        spdlog::warn("Tree {}: Command failed - {}", id, result.message);
    }
}

void Tree::decideNextAction(const World& world)
{
    // Gather sensory data and ask brain for next command.
    TreeSensoryData sensory = gatherSensoryData(world);
    TreeCommand command = brain_->decide(sensory);

    // Enqueue command.
    current_command = command;

    std::visit(
        [&](auto&& cmd) {
            using T = std::decay_t<decltype(cmd)>;

            if constexpr (std::is_same_v<T, GrowWoodCommand>) {
                time_remaining_seconds = cmd.execution_time_seconds;
            }
            else if constexpr (std::is_same_v<T, GrowLeafCommand>) {
                time_remaining_seconds = cmd.execution_time_seconds;
            }
            else if constexpr (std::is_same_v<T, GrowRootCommand>) {
                time_remaining_seconds = cmd.execution_time_seconds;
            }
            else if constexpr (std::is_same_v<T, ReinforceCellCommand>) {
                time_remaining_seconds = cmd.execution_time_seconds;
            }
            else if constexpr (std::is_same_v<T, ProduceSeedCommand>) {
                time_remaining_seconds = cmd.execution_time_seconds;
            }
            else if constexpr (std::is_same_v<T, WaitCommand>) {
                time_remaining_seconds = cmd.duration_seconds;
            }
        },
        command);
}

void Tree::updateResources(const World& world)
{
    // TODO: Implement resource aggregation from cells.
    // For now, just placeholder values.
    (void)world;

    // Phase 3 will implement:
    // - Iterate over cells, sum energy and water from cell metadata.
    // - Calculate photosynthesis from LEAF cells based on light exposure.
    // - Extract nutrients from DIRT via ROOT cells.
    // - Deduct maintenance costs.
}

TreeSensoryData Tree::gatherSensoryData(const World& world) const
{
    TreeSensoryData data;

    // Find actual current cell positions by scanning world for organism_id.
    // This handles cells that have moved due to physics (falling seeds).
    int min_x = INT32_MAX, min_y = INT32_MAX;
    int max_x = INT32_MIN, max_y = INT32_MIN;
    int cell_count = 0;

    for (uint32_t y = 0; y < world.getData().height; y++) {
        for (uint32_t x = 0; x < world.getData().width; x++) {
            if (world.getData().at(x, y).organism_id == id) {
                min_x = std::min(min_x, static_cast<int>(x));
                min_y = std::min(min_y, static_cast<int>(y));
                max_x = std::max(max_x, static_cast<int>(x));
                max_y = std::max(max_y, static_cast<int>(y));
                cell_count++;
            }
        }
    }

    // No cells found - tree might have been destroyed.
    if (cell_count == 0) {
        data.actual_width = TreeSensoryData::GRID_SIZE;
        data.actual_height = TreeSensoryData::GRID_SIZE;
        data.scale_factor = 1.0;
        data.world_offset = Vector2i{ 0, 0 };
        return data;
    }

    int bbox_width = max_x - min_x + 1;
    int bbox_height = max_y - min_y + 1;

    // Small trees: Use fixed 15×15 viewing window centered on tree's current position (1:1
    // mapping).
    if (bbox_width <= TreeSensoryData::GRID_SIZE && bbox_height <= TreeSensoryData::GRID_SIZE) {
        data.actual_width = TreeSensoryData::GRID_SIZE;
        data.actual_height = TreeSensoryData::GRID_SIZE;
        data.scale_factor = 1.0;

        // Center the 15×15 window on the original seed position (fixed anchor).
        int half_window = TreeSensoryData::GRID_SIZE / 2; // 7 cells on each side.
        int offset_x = seed_position.x - half_window;
        int offset_y = seed_position.y - half_window;

        // Clamp to world bounds (allow negative offsets for small worlds).
        // For worlds >= 15×15: clamp to keep window inside world.
        // For worlds < 15×15: allow negative offset to center seed in neural grid.
        if (static_cast<int>(world.getData().width) >= TreeSensoryData::GRID_SIZE) {
            offset_x = std::max(
                0,
                std::min(
                    static_cast<int>(world.getData().width) - TreeSensoryData::GRID_SIZE,
                    offset_x));
        }
        // else: leave offset_x as calculated (may be negative)

        if (static_cast<int>(world.getData().height) >= TreeSensoryData::GRID_SIZE) {
            offset_y = std::max(
                0,
                std::min(
                    static_cast<int>(world.getData().height) - TreeSensoryData::GRID_SIZE,
                    offset_y));
        }
        // else: leave offset_y as calculated (may be negative)

        data.world_offset = Vector2i{ offset_x, offset_y };
    }
    // Large trees: Use bounding box + padding, downsample to fit 15×15.
    else {
        // Add 1-cell padding.
        min_x = std::max(0, min_x - 1);
        min_y = std::max(0, min_y - 1);
        max_x = std::min(static_cast<int>(world.getData().width) - 1, max_x + 1);
        max_y = std::min(static_cast<int>(world.getData().height) - 1, max_y + 1);

        data.actual_width = max_x - min_x + 1;
        data.actual_height = max_y - min_y + 1;
        data.world_offset = Vector2i{ min_x, min_y };
        data.scale_factor = std::max(
            static_cast<double>(data.actual_width) / TreeSensoryData::GRID_SIZE,
            static_cast<double>(data.actual_height) / TreeSensoryData::GRID_SIZE);
    }

    // Populate material histograms by sampling world grid.
    for (int ny = 0; ny < TreeSensoryData::GRID_SIZE; ny++) {
        for (int nx = 0; nx < TreeSensoryData::GRID_SIZE; nx++) {
            // Map neural coords to world region.
            int wx_start = data.world_offset.x + static_cast<int>(nx * data.scale_factor);
            int wy_start = data.world_offset.y + static_cast<int>(ny * data.scale_factor);
            int wx_end = data.world_offset.x + static_cast<int>((nx + 1) * data.scale_factor);
            int wy_end = data.world_offset.y + static_cast<int>((ny + 1) * data.scale_factor);

            // Check if region is completely out of bounds (skip sampling to get empty histogram).
            if (wx_end <= 0 || wx_start >= static_cast<int>(world.getData().width) || wy_end <= 0
                || wy_start >= static_cast<int>(world.getData().height)) {
                // Completely OOB - leave histogram as zeros (will render as AIR/black).
                continue;
            }

            // Clamp to world bounds.
            wx_start = std::max(0, std::min(static_cast<int>(world.getData().width) - 1, wx_start));
            wy_start =
                std::max(0, std::min(static_cast<int>(world.getData().height) - 1, wy_start));
            wx_end = std::max(0, std::min(static_cast<int>(world.getData().width), wx_end));
            wy_end = std::max(0, std::min(static_cast<int>(world.getData().height), wy_end));

            // Count materials in this region.
            std::array<int, TreeSensoryData::NUM_MATERIALS> counts = {};
            int total_cells = 0;

            for (int wy = wy_start; wy < wy_end; wy++) {
                for (int wx = wx_start; wx < wx_end; wx++) {
                    const auto& cell = world.getData().at(wx, wy);
                    int mat_idx = static_cast<int>(cell.material_type);
                    if (mat_idx >= 0 && mat_idx < TreeSensoryData::NUM_MATERIALS) {
                        counts[mat_idx]++;
                        total_cells++;
                    }
                }
            }

            // Normalize to histogram probabilities.
            if (total_cells > 0) {
                for (int i = 0; i < TreeSensoryData::NUM_MATERIALS; i++) {
                    data.material_histograms[ny][nx][i] =
                        static_cast<double>(counts[i]) / total_cells;
                }
            }
        }
    }

    data.seed_position = seed_position;
    data.age_seconds = age_seconds;
    data.stage = stage;
    data.total_energy = total_energy;
    data.total_water = total_water;

    if (current_command.has_value()) {
        std::visit(
            [&](auto&& cmd) {
                using T = std::decay_t<decltype(cmd)>;
                if constexpr (std::is_same_v<T, GrowWoodCommand>) {
                    data.current_thought = "Growing WOOD at (" + std::to_string(cmd.target_pos.x)
                        + ", " + std::to_string(cmd.target_pos.y) + ")";
                }
                else if constexpr (std::is_same_v<T, GrowLeafCommand>) {
                    data.current_thought = "Growing LEAF at (" + std::to_string(cmd.target_pos.x)
                        + ", " + std::to_string(cmd.target_pos.y) + ")";
                }
                else if constexpr (std::is_same_v<T, GrowRootCommand>) {
                    data.current_thought = "Growing ROOT at (" + std::to_string(cmd.target_pos.x)
                        + ", " + std::to_string(cmd.target_pos.y) + ")";
                }
                else if constexpr (std::is_same_v<T, ReinforceCellCommand>) {
                    data.current_thought = "Reinforcing cell at (" + std::to_string(cmd.position.x)
                        + ", " + std::to_string(cmd.position.y) + ")";
                }
                else if constexpr (std::is_same_v<T, ProduceSeedCommand>) {
                    data.current_thought = "Producing SEED at (" + std::to_string(cmd.position.x)
                        + ", " + std::to_string(cmd.position.y) + ")";
                }
                else if constexpr (std::is_same_v<T, WaitCommand>) {
                    data.current_thought = "Waiting";
                }
            },
            *current_command);
    }
    else {
        data.current_thought = "Idle";
    }

    return data;
}

} // namespace DirtSim
