#include "Tree.h"
#include "core/MaterialType.h"
#include "core/World.h"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace DirtSim {

Tree::Tree(TreeId id, std::unique_ptr<TreeBrain> brain) : id(id), brain_(std::move(brain))
{}

void Tree::update(World& world, double deltaTime)
{
    (void)deltaTime; // Trees use discrete timesteps, not continuous time.

    age++;

    // Execute current command (tick down timer).
    if (current_command.has_value()) {
        if (--steps_remaining == 0) {
            executeCommand(world);
            current_command.reset();
        }
    }

    // When idle, ask brain for next action.
    if (!current_command.has_value()) {
        decideNextAction(world);
    }

    // Resource updates (continuous).
    updateResources(world);
}

void Tree::executeCommand(World& world)
{
    std::visit(
        [&](auto&& cmd) {
            using T = std::decay_t<decltype(cmd)>;

            if constexpr (std::is_same_v<T, GrowWoodCommand>) {
                // Check if target position is valid and adjacent to tree.
                if (cmd.target_pos.x < 0 || cmd.target_pos.y < 0
                    || static_cast<uint32_t>(cmd.target_pos.x) >= world.data.width
                    || static_cast<uint32_t>(cmd.target_pos.y) >= world.data.height) {
                    spdlog::warn(
                        "Tree {}: GrowWoodCommand invalid position ({}, {})",
                        id,
                        cmd.target_pos.x,
                        cmd.target_pos.y);
                    return;
                }

                // TODO: Validate adjacency to existing tree cells.
                // TODO: Check energy availability.

                // Replace target cell with WOOD.
                world.addMaterialAtCell(
                    cmd.target_pos.x, cmd.target_pos.y, MaterialType::WOOD, 1.0);

                // Mark cell as owned by this tree.
                world.at(cmd.target_pos.x, cmd.target_pos.y).organism_id = id;

                cells.insert(cmd.target_pos);
                total_energy -= cmd.energy_cost;

                spdlog::info(
                    "Tree {}: Grew WOOD at ({}, {})", id, cmd.target_pos.x, cmd.target_pos.y);

                // Stage transition: SEED → GERMINATION when first wood appears.
                if (stage == GrowthStage::SEED) {
                    stage = GrowthStage::GERMINATION;
                    spdlog::info("Tree {}: Transitioned to GERMINATION stage", id);
                }
            }
            else if constexpr (std::is_same_v<T, GrowLeafCommand>) {
                if (cmd.target_pos.x < 0 || cmd.target_pos.y < 0
                    || static_cast<uint32_t>(cmd.target_pos.x) >= world.data.width
                    || static_cast<uint32_t>(cmd.target_pos.y) >= world.data.height) {
                    spdlog::warn(
                        "Tree {}: GrowLeafCommand invalid position ({}, {})",
                        id,
                        cmd.target_pos.x,
                        cmd.target_pos.y);
                    return;
                }

                world.addMaterialAtCell(
                    cmd.target_pos.x, cmd.target_pos.y, MaterialType::LEAF, 1.0);

                // Mark cell as owned by this tree.
                world.at(cmd.target_pos.x, cmd.target_pos.y).organism_id = id;

                cells.insert(cmd.target_pos);
                total_energy -= cmd.energy_cost;

                spdlog::info(
                    "Tree {}: Grew LEAF at ({}, {})", id, cmd.target_pos.x, cmd.target_pos.y);
            }
            else if constexpr (std::is_same_v<T, GrowRootCommand>) {
                if (cmd.target_pos.x < 0 || cmd.target_pos.y < 0
                    || static_cast<uint32_t>(cmd.target_pos.x) >= world.data.width
                    || static_cast<uint32_t>(cmd.target_pos.y) >= world.data.height) {
                    spdlog::warn(
                        "Tree {}: GrowRootCommand invalid position ({}, {})",
                        id,
                        cmd.target_pos.x,
                        cmd.target_pos.y);
                    return;
                }

                // TODO: Add ROOT material type first.
                // For now, use WOOD as placeholder.
                world.addMaterialAtCell(
                    cmd.target_pos.x, cmd.target_pos.y, MaterialType::WOOD, 1.0);

                // Mark cell as owned by this tree.
                world.at(cmd.target_pos.x, cmd.target_pos.y).organism_id = id;

                cells.insert(cmd.target_pos);
                total_energy -= cmd.energy_cost;

                spdlog::info(
                    "Tree {}: Grew ROOT at ({}, {}) [using WOOD placeholder]",
                    id,
                    cmd.target_pos.x,
                    cmd.target_pos.y);

                // Stage transition: GERMINATION → SAPLING when first root appears.
                if (stage == GrowthStage::GERMINATION) {
                    stage = GrowthStage::SAPLING;
                    spdlog::info("Tree {}: Transitioned to SAPLING stage", id);
                }
            }
            else if constexpr (std::is_same_v<T, ReinforceCellCommand>) {
                // TODO: Implement cell reinforcement once we have structural integrity tracking.
                spdlog::info(
                    "Tree {}: Reinforced cell at ({}, {}) [not yet implemented]",
                    id,
                    cmd.position.x,
                    cmd.position.y);
                total_energy -= cmd.energy_cost;
            }
            else if constexpr (std::is_same_v<T, ProduceSeedCommand>) {
                if (cmd.position.x < 0 || cmd.position.y < 0
                    || static_cast<uint32_t>(cmd.position.x) >= world.data.width
                    || static_cast<uint32_t>(cmd.position.y) >= world.data.height) {
                    spdlog::warn(
                        "Tree {}: ProduceSeedCommand invalid position ({}, {})",
                        id,
                        cmd.position.x,
                        cmd.position.y);
                    return;
                }

                world.addMaterialAtCell(cmd.position.x, cmd.position.y, MaterialType::SEED, 1.0);

                total_energy -= cmd.energy_cost;

                spdlog::info(
                    "Tree {}: Produced SEED at ({}, {})", id, cmd.position.x, cmd.position.y);
            }
            else if constexpr (std::is_same_v<T, WaitCommand>) {
                // Intentionally empty - just idle.
                spdlog::debug("Tree {}: Waited for {} steps", id, cmd.duration);
            }
        },
        *current_command);
}

void Tree::decideNextAction(const World& world)
{
    // Gather sensory data and ask brain for next command.
    TreeSensoryData sensory = gatherSensoryData(world);
    TreeCommand command = brain_->decide(sensory);

    // Enqueue command.
    current_command = command;

    // Extract execution time from command.
    std::visit(
        [&](auto&& cmd) {
            using T = std::decay_t<decltype(cmd)>;

            if constexpr (std::is_same_v<T, GrowWoodCommand>) {
                steps_remaining = cmd.execution_time;
            }
            else if constexpr (std::is_same_v<T, GrowLeafCommand>) {
                steps_remaining = cmd.execution_time;
            }
            else if constexpr (std::is_same_v<T, GrowRootCommand>) {
                steps_remaining = cmd.execution_time;
            }
            else if constexpr (std::is_same_v<T, ReinforceCellCommand>) {
                steps_remaining = cmd.execution_time;
            }
            else if constexpr (std::is_same_v<T, ProduceSeedCommand>) {
                steps_remaining = cmd.execution_time;
            }
            else if constexpr (std::is_same_v<T, WaitCommand>) {
                steps_remaining = cmd.duration;
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

    for (uint32_t y = 0; y < world.data.height; y++) {
        for (uint32_t x = 0; x < world.data.width; x++) {
            if (world.at(x, y).organism_id == id) {
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

    // Calculate geometric center of tree's current cells.
    int center_x = (min_x + max_x) / 2;
    int center_y = (min_y + max_y) / 2;

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

        // Clamp to world bounds.
        offset_x = std::max(
            0, std::min(static_cast<int>(world.data.width) - TreeSensoryData::GRID_SIZE, offset_x));
        offset_y = std::max(
            0,
            std::min(static_cast<int>(world.data.height) - TreeSensoryData::GRID_SIZE, offset_y));

        data.world_offset = Vector2i{ offset_x, offset_y };
    }
    // Large trees: Use bounding box + padding, downsample to fit 15×15.
    else {
        // Add 1-cell padding.
        min_x = std::max(0, min_x - 1);
        min_y = std::max(0, min_y - 1);
        max_x = std::min(static_cast<int>(world.data.width) - 1, max_x + 1);
        max_y = std::min(static_cast<int>(world.data.height) - 1, max_y + 1);

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

            // Clamp to world bounds.
            wx_start = std::max(0, std::min(static_cast<int>(world.data.width) - 1, wx_start));
            wy_start = std::max(0, std::min(static_cast<int>(world.data.height) - 1, wy_start));
            wx_end = std::max(0, std::min(static_cast<int>(world.data.width), wx_end));
            wy_end = std::max(0, std::min(static_cast<int>(world.data.height), wy_end));

            // Count materials in this region.
            std::array<int, TreeSensoryData::NUM_MATERIALS> counts = {};
            int total_cells = 0;

            for (int wy = wy_start; wy < wy_end; wy++) {
                for (int wx = wx_start; wx < wx_end; wx++) {
                    const auto& cell = world.at(wx, wy);
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

    // Populate internal state.
    data.age = age;
    data.stage = stage;
    data.total_energy = total_energy;
    data.total_water = total_water;
    data.root_count = 0; // TODO: Count ROOT cells.
    data.leaf_count = 0; // TODO: Count LEAF cells.
    data.wood_count = 0; // TODO: Count WOOD cells.

    return data;
}

} // namespace DirtSim
