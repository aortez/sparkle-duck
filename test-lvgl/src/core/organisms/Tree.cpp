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

    // TODO: Implement scale-invariant sensory data gathering.
    // For now, provide minimal data for brain.

    // Calculate bounding box of tree cells.
    if (cells.empty()) {
        // Single-cell SEED tree.
        data.actual_width = 1;
        data.actual_height = 1;
        data.scale_factor = 1.0 / TreeSensoryData::GRID_SIZE;
        data.world_offset = Vector2i{ 0, 0 };
    }
    else {
        // Find bounding box.
        int min_x = INT32_MAX, min_y = INT32_MAX;
        int max_x = INT32_MIN, max_y = INT32_MIN;

        for (const auto& pos : cells) {
            min_x = std::min(min_x, pos.x);
            min_y = std::min(min_y, pos.y);
            max_x = std::max(max_x, pos.x);
            max_y = std::max(max_y, pos.y);
        }

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

        // TODO: Populate material_histograms by sampling world grid.
        // For now, leave zeroed (Phase 2 will implement).
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
