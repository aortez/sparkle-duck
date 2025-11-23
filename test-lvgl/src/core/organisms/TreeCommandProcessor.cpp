#include "TreeCommandProcessor.h"
#include "Tree.h"
#include "core/Cell.h"
#include "core/MaterialType.h"
#include "core/World.h"
#include "core/WorldData.h"
#include <spdlog/spdlog.h>

namespace DirtSim {

CommandExecutionResult TreeCommandProcessor::execute(
    Tree& tree, World& world, const TreeCommand& cmd)
{
    return std::visit(
        [&](auto&& command) -> CommandExecutionResult {
            using T = std::decay_t<decltype(command)>;

            if constexpr (std::is_same_v<T, GrowWoodCommand>) {
                if (tree.total_energy < command.energy_cost) {
                    return { CommandResult::INSUFFICIENT_ENERGY,
                             "Not enough energy for WOOD growth" };
                }

                if (command.target_pos.x < 0 || command.target_pos.y < 0
                    || static_cast<uint32_t>(command.target_pos.x) >= world.getData().width
                    || static_cast<uint32_t>(command.target_pos.y) >= world.getData().height) {
                    return { CommandResult::INVALID_TARGET, "WOOD target out of bounds" };
                }

                // Check cardinal adjacency to WOOD or SEED (structural elements only).
                Vector2i cardinal_dirs[] = { { 0, 1 }, { 0, -1 }, { -1, 0 }, { 1, 0 } };
                bool has_structural_neighbor = false;
                for (const auto& dir : cardinal_dirs) {
                    Vector2i neighbor_pos = command.target_pos + dir;
                    if (neighbor_pos.x >= 0 && neighbor_pos.y >= 0
                        && static_cast<uint32_t>(neighbor_pos.x) < world.getData().width
                        && static_cast<uint32_t>(neighbor_pos.y) < world.getData().height) {
                        const Cell& neighbor = world.at(neighbor_pos.x, neighbor_pos.y);
                        if (neighbor.organism_id == tree.id
                            && (neighbor.material_type == MaterialType::WOOD
                                || neighbor.material_type == MaterialType::SEED)) {
                            has_structural_neighbor = true;
                            break;
                        }
                    }
                }

                if (!has_structural_neighbor) {
                    return { CommandResult::INVALID_TARGET,
                             "WOOD requires cardinal adjacency to WOOD or SEED" };
                }

                world.at(command.target_pos.x, command.target_pos.y)
                    .replaceMaterial(MaterialType::WOOD, 1.0);
                world.at(command.target_pos.x, command.target_pos.y).organism_id = tree.id;

                tree.cells.insert(command.target_pos);
                tree.total_energy -= command.energy_cost;

                spdlog::info(
                    "Tree {}: Grew WOOD at ({}, {})",
                    tree.id,
                    command.target_pos.x,
                    command.target_pos.y);

                if (tree.stage == GrowthStage::GERMINATION) {
                    tree.stage = GrowthStage::SAPLING;
                    spdlog::info("Tree {}: Transitioned to SAPLING stage", tree.id);
                }

                return { CommandResult::SUCCESS, "WOOD growth successful" };
            }
            else if constexpr (std::is_same_v<T, GrowLeafCommand>) {
                if (tree.total_energy < command.energy_cost) {
                    return { CommandResult::INSUFFICIENT_ENERGY,
                             "Not enough energy for LEAF growth" };
                }

                if (command.target_pos.x < 0 || command.target_pos.y < 0
                    || static_cast<uint32_t>(command.target_pos.x) >= world.getData().width
                    || static_cast<uint32_t>(command.target_pos.y) >= world.getData().height) {
                    return { CommandResult::INVALID_TARGET, "LEAF target out of bounds" };
                }

                // Check cardinal adjacency to WOOD (leaves grow from branches).
                Vector2i cardinal_dirs[] = { { 0, 1 }, { 0, -1 }, { -1, 0 }, { 1, 0 } };
                bool has_wood_neighbor = false;
                for (const auto& dir : cardinal_dirs) {
                    Vector2i neighbor_pos = command.target_pos + dir;
                    if (neighbor_pos.x >= 0 && neighbor_pos.y >= 0
                        && static_cast<uint32_t>(neighbor_pos.x) < world.getData().width
                        && static_cast<uint32_t>(neighbor_pos.y) < world.getData().height) {
                        const Cell& neighbor = world.at(neighbor_pos.x, neighbor_pos.y);
                        if (neighbor.organism_id == tree.id
                            && neighbor.material_type == MaterialType::WOOD) {
                            has_wood_neighbor = true;
                            break;
                        }
                    }
                }

                if (!has_wood_neighbor) {
                    return { CommandResult::INVALID_TARGET,
                             "LEAF requires cardinal adjacency to WOOD" };
                }

                world.at(command.target_pos.x, command.target_pos.y)
                    .replaceMaterial(MaterialType::LEAF, 1.0);
                world.at(command.target_pos.x, command.target_pos.y).organism_id = tree.id;

                tree.cells.insert(command.target_pos);
                tree.total_energy -= command.energy_cost;

                spdlog::info(
                    "Tree {}: Grew LEAF at ({}, {})",
                    tree.id,
                    command.target_pos.x,
                    command.target_pos.y);

                return { CommandResult::SUCCESS, "LEAF growth successful" };
            }
            else if constexpr (std::is_same_v<T, GrowRootCommand>) {
                if (tree.total_energy < command.energy_cost) {
                    return { CommandResult::INSUFFICIENT_ENERGY,
                             "Not enough energy for ROOT growth" };
                }

                if (command.target_pos.x < 0 || command.target_pos.y < 0
                    || static_cast<uint32_t>(command.target_pos.x) >= world.getData().width
                    || static_cast<uint32_t>(command.target_pos.y) >= world.getData().height) {
                    return { CommandResult::INVALID_TARGET, "ROOT target out of bounds" };
                }

                // Check cardinal adjacency to SEED or ROOT (root network).
                Vector2i cardinal_dirs[] = { { 0, 1 }, { 0, -1 }, { -1, 0 }, { 1, 0 } };
                bool has_root_neighbor = false;
                for (const auto& dir : cardinal_dirs) {
                    Vector2i neighbor_pos = command.target_pos + dir;
                    if (neighbor_pos.x >= 0 && neighbor_pos.y >= 0
                        && static_cast<uint32_t>(neighbor_pos.x) < world.getData().width
                        && static_cast<uint32_t>(neighbor_pos.y) < world.getData().height) {
                        const Cell& neighbor = world.at(neighbor_pos.x, neighbor_pos.y);
                        if (neighbor.organism_id == tree.id
                            && (neighbor.material_type == MaterialType::ROOT
                                || neighbor.material_type == MaterialType::SEED)) {
                            has_root_neighbor = true;
                            break;
                        }
                    }
                }

                if (!has_root_neighbor) {
                    return { CommandResult::INVALID_TARGET,
                             "ROOT requires cardinal adjacency to SEED or ROOT" };
                }

                world.at(command.target_pos.x, command.target_pos.y)
                    .replaceMaterial(MaterialType::ROOT, 1.0);
                world.at(command.target_pos.x, command.target_pos.y).organism_id = tree.id;

                tree.cells.insert(command.target_pos);
                tree.total_energy -= command.energy_cost;

                spdlog::info(
                    "Tree {}: Grew ROOT at ({}, {})",
                    tree.id,
                    command.target_pos.x,
                    command.target_pos.y);

                if (tree.stage == GrowthStage::SEED) {
                    tree.stage = GrowthStage::GERMINATION;
                    spdlog::info("Tree {}: Transitioned to GERMINATION stage", tree.id);
                }

                return { CommandResult::SUCCESS, "ROOT growth successful" };
            }
            else if constexpr (std::is_same_v<T, ReinforceCellCommand>) {
                if (tree.total_energy < command.energy_cost) {
                    return { CommandResult::INSUFFICIENT_ENERGY,
                             "Not enough energy for cell reinforcement" };
                }

                tree.total_energy -= command.energy_cost;

                spdlog::info(
                    "Tree {}: Reinforced cell at ({}, {}) [not yet implemented]",
                    tree.id,
                    command.position.x,
                    command.position.y);

                return { CommandResult::SUCCESS, "Cell reinforcement successful" };
            }
            else if constexpr (std::is_same_v<T, ProduceSeedCommand>) {
                if (tree.total_energy < command.energy_cost) {
                    return { CommandResult::INSUFFICIENT_ENERGY,
                             "Not enough energy for seed production" };
                }

                if (command.position.x < 0 || command.position.y < 0
                    || static_cast<uint32_t>(command.position.x) >= world.getData().width
                    || static_cast<uint32_t>(command.position.y) >= world.getData().height) {
                    return { CommandResult::INVALID_TARGET, "Seed position out of bounds" };
                }

                world.at(command.position.x, command.position.y)
                    .replaceMaterial(MaterialType::SEED, 1.0);

                tree.total_energy -= command.energy_cost;

                spdlog::info(
                    "Tree {}: Produced SEED at ({}, {})",
                    tree.id,
                    command.position.x,
                    command.position.y);

                return { CommandResult::SUCCESS, "Seed production successful" };
            }
            else if constexpr (std::is_same_v<T, WaitCommand>) {
                spdlog::debug("Tree {}: Waited for {} seconds", tree.id, command.duration_seconds);
                return { CommandResult::SUCCESS, "Wait completed" };
            }

            return { CommandResult::INVALID_TARGET, "Unknown command type" };
        },
        cmd);
}

} // namespace DirtSim
