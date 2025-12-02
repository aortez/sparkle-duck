#include "TreeManager.h"
#include "brains/RuleBasedBrain.h"
#include "core/Cell.h"
#include "core/GridOfCells.h"
#include "core/LoggingChannels.h"
#include "core/MaterialType.h"
#include "core/World.h"
#include "core/WorldData.h"
#include <algorithm>
#include <queue>
#include <random>
#include <unordered_set>

namespace DirtSim {

void TreeManager::update(World& world, double deltaTime)
{
    for (auto& [id, tree] : trees_) {
        tree.update(world, deltaTime);
    }
}

TreeId TreeManager::plantSeed(World& world, uint32_t x, uint32_t y)
{
    TreeId id = next_tree_id_++;

    auto brain = std::make_unique<RuleBasedBrain>();
    Tree tree(id, std::move(brain));

    Vector2i pos{ static_cast<int>(x), static_cast<int>(y) };
    tree.seed_position = pos;
    tree.total_energy = 150.0; // Starting energy for tree growth.

    world.addMaterialAtCell(x, y, MaterialType::SEED, 1.0);

    tree.cells.insert(pos);
    cell_to_tree_[pos] = id;

    world.getData().at(x, y).organism_id = id;

    LoggingChannels::tree()->info("TreeManager: Planted seed for tree {} at ({}, {})", id, x, y);

    trees_.emplace(id, std::move(tree));

    return id;
}

void TreeManager::removeTree(TreeId id)
{
    auto it = trees_.find(id);
    if (it == trees_.end()) {
        LoggingChannels::tree()->warn("TreeManager: Attempted to remove non-existent tree {}", id);
        return;
    }

    // Remove cell ownership tracking.
    for (const auto& pos : it->second.cells) {
        cell_to_tree_.erase(pos);
    }

    // Remove tree.
    trees_.erase(it);

    LoggingChannels::tree()->info("TreeManager: Removed tree {}", id);
}

Tree* TreeManager::getTree(TreeId id)
{
    auto it = trees_.find(id);
    return it != trees_.end() ? &it->second : nullptr;
}

const Tree* TreeManager::getTree(TreeId id) const
{
    auto it = trees_.find(id);
    return it != trees_.end() ? &it->second : nullptr;
}

TreeId TreeManager::getTreeAtCell(const Vector2i& pos) const
{
    auto it = cell_to_tree_.find(pos);
    return it != cell_to_tree_.end() ? it->second : INVALID_TREE_ID;
}

void TreeManager::notifyTransfers(const std::vector<OrganismTransfer>& transfers)
{
    if (!transfers.empty()) {
        spdlog::info("TreeManager::notifyTransfers called with {} transfers", transfers.size());
    }

    // Batch transfers by tree ID for efficient processing.
    std::unordered_map<TreeId, std::vector<const OrganismTransfer*>> transfers_by_tree;

    for (const auto& transfer : transfers) {
        transfers_by_tree[transfer.organism_id].push_back(&transfer);
    }

    // Update each affected tree's cell tracking.
    for (const auto& [tree_id, tree_transfers] : transfers_by_tree) {
        auto tree_it = trees_.find(tree_id);
        if (tree_it == trees_.end()) {
            LoggingChannels::tree()->warn(
                "TreeManager: Received transfers for non-existent tree {}", tree_id);
            continue;
        }

        Tree& tree = tree_it->second;

        for (const OrganismTransfer* transfer : tree_transfers) {
            // Add destination to tree's cell set.
            tree.cells.insert(transfer->to_pos);
            cell_to_tree_[transfer->to_pos] = tree_id;

            // If the seed cell is moving, update seed_position to track it.
            if (transfer->from_pos == tree.seed_position) {
                tree.seed_position = transfer->to_pos;
                LoggingChannels::tree()->debug(
                    "TreeManager: Tree {} seed moved from ({}, {}) to ({}, {})",
                    tree_id,
                    transfer->from_pos.x,
                    transfer->from_pos.y,
                    transfer->to_pos.x,
                    transfer->to_pos.y);
            }

            // Update bone endpoints when cells move.
            // Any bone referencing from_pos now needs to reference to_pos.
            spdlog::info(
                "TreeManager: Processing transfer ({},{}) -> ({},{}) for tree {} with {} bones",
                transfer->from_pos.x,
                transfer->from_pos.y,
                transfer->to_pos.x,
                transfer->to_pos.y,
                tree_id,
                tree.bones.size());

            for (Bone& bone : tree.bones) {
                if (bone.cell_a == transfer->from_pos) {
                    bone.cell_a = transfer->to_pos;
                    spdlog::info(
                        "TreeManager: Updated bone cell_a from ({},{}) to ({},{})",
                        transfer->from_pos.x,
                        transfer->from_pos.y,
                        transfer->to_pos.x,
                        transfer->to_pos.y);
                }
                if (bone.cell_b == transfer->from_pos) {
                    bone.cell_b = transfer->to_pos;
                    spdlog::info(
                        "TreeManager: Updated bone cell_b from ({},{}) to ({},{})",
                        transfer->from_pos.x,
                        transfer->from_pos.y,
                        transfer->to_pos.x,
                        transfer->to_pos.y);
                }
            }

            // Note: We don't remove from_pos yet - source cell might still have material.
            // The cleanup will happen in a separate pass or when cell becomes fully empty.
        }

        LoggingChannels::tree()->trace(
            "TreeManager: Processed {} transfers for tree {} (now {} cells tracked)",
            tree_transfers.size(),
            tree_id,
            tree.cells.size());
    }
}

void TreeManager::computeOrganismSupport(World& world)
{
    WorldData& data = world.getData();
    GridOfCells& grid = world.getGrid();

    for (auto& [tree_id, tree] : trees_) {
        // Step 1: Calculate root anchoring budget.
        double support_budget = 0.0;
        int root_count = 0;

        for (const auto& pos : tree.cells) {
            if (static_cast<uint32_t>(pos.x) >= data.width
                || static_cast<uint32_t>(pos.y) >= data.height)
                continue;

            const Cell& cell = data.at(pos.x, pos.y);
            if (cell.organism_id != tree_id || cell.material_type != MaterialType::ROOT) {
                continue;
            }

            root_count++;
            double root_anchoring = 0.0;
            int dirt_neighbors = 0;

            // Check all 8 neighbors for non-tree material to grip.
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;

                    int nx = pos.x + dx;
                    int ny = pos.y + dy;

                    if (nx < 0 || ny < 0 || nx >= static_cast<int>(data.width)
                        || ny >= static_cast<int>(data.height)) {
                        continue;
                    }

                    const Cell& neighbor = data.at(nx, ny);

                    // Only count non-tree cells (dirt, sand, etc. that roots grip).
                    if (neighbor.organism_id != tree_id && !neighbor.isEmpty()) {
                        const MaterialProperties& neighbor_props =
                            getMaterialProperties(neighbor.material_type);
                        double neighbor_mass = neighbor.fill_ratio * neighbor_props.density;

                        // Use neighbor's adhesion (how well material sticks to roots).
                        double contribution = neighbor_mass * neighbor_props.adhesion;
                        root_anchoring += contribution;

                        if (neighbor.material_type == MaterialType::DIRT) {
                            dirt_neighbors++;
                        }

                        LoggingChannels::tree()->debug(
                            "  ROOT({},{}) neighbor({},{}) {} mass={:.2f} adhesion={:.2f} "
                            "contrib={:.2f}",
                            pos.x,
                            pos.y,
                            nx,
                            ny,
                            getMaterialName(neighbor.material_type),
                            neighbor_mass,
                            neighbor_props.adhesion,
                            contribution);
                    }
                }
            }

            LoggingChannels::tree()->debug(
                "ROOT at ({},{}) has {} dirt neighbors, anchoring={:.2f}",
                pos.x,
                pos.y,
                dirt_neighbors,
                root_anchoring);

            support_budget += root_anchoring;
        }

        support_budget *= 2.0;

        LoggingChannels::tree()->debug(
            "Tree {} support calculation: {} roots, total_budget={:.2f}",
            tree_id,
            root_count,
            support_budget);

        // Step 2: Calculate upper structure mass (everything except ROOTs).
        std::vector<Vector2i> upper_cells;
        double upper_mass = 0.0;

        for (const auto& pos : tree.cells) {
            if (static_cast<uint32_t>(pos.x) >= data.width
                || static_cast<uint32_t>(pos.y) >= data.height)
                continue;

            const Cell& cell = data.at(pos.x, pos.y);
            if (cell.organism_id != tree_id) continue;

            // Skip ROOT cells - they provide support, don't consume it.
            if (cell.material_type == MaterialType::ROOT) continue;

            upper_cells.push_back(pos);
            const MaterialProperties& props = getMaterialProperties(cell.material_type);
            double cell_mass = cell.fill_ratio * props.density;
            upper_mass += cell_mass;
        }

        // Step 2.5: Grant support to LEAF cells adjacent to same-organism WOOD.
        // LEAFs are supported by physical attachment to branches, not by root budget.
        for (const auto& pos : tree.cells) {
            if (static_cast<uint32_t>(pos.x) >= data.width
                || static_cast<uint32_t>(pos.y) >= data.height)
                continue;

            Cell& cell = data.at(pos.x, pos.y);
            if (cell.organism_id != tree_id || cell.material_type != MaterialType::LEAF) continue;

            if (cell.has_any_support) continue; // Already supported.

            // Check cardinal neighbors for same-organism WOOD.
            static constexpr Vector2i cardinal_dirs[] = {
                { 0, 1 }, { 0, -1 }, { -1, 0 }, { 1, 0 }
            };
            for (const auto& dir : cardinal_dirs) {
                int nx = pos.x + dir.x;
                int ny = pos.y + dir.y;

                if (nx < 0 || ny < 0 || nx >= static_cast<int>(data.width)
                    || ny >= static_cast<int>(data.height))
                    continue;

                const Cell& neighbor = data.at(nx, ny);
                if (neighbor.organism_id == tree_id
                    && neighbor.material_type == MaterialType::WOOD) {
                    // LEAF is attached to same-organism WOOD - grant full support.
                    // Use vertical support flag to get full resistance (support_factor=1.0).
                    cell.has_any_support = true;
                    cell.has_vertical_support = true;
                    if (GridOfCells::USE_CACHE) {
                        grid.supportBitmap().set(pos.x, pos.y);
                    }
                    LoggingChannels::tree()->debug(
                        "TreeManager: LEAF at ({},{}) rigidly supported by adjacent WOOD at "
                        "({},{})",
                        pos.x,
                        pos.y,
                        nx,
                        ny);
                    break;
                }
            }
        }

        // Step 2.7: Grant support to ROOT cells surrounded by dirt (anchored in soil).
        for (const auto& pos : tree.cells) {
            if (static_cast<uint32_t>(pos.x) >= data.width
                || static_cast<uint32_t>(pos.y) >= data.height)
                continue;

            Cell& cell = data.at(pos.x, pos.y);
            if (cell.organism_id != tree_id || cell.material_type != MaterialType::ROOT) continue;
            if (cell.has_any_support) continue; // Already supported.

            // Check for adjacent dirt/sand (roots grip soil).
            static constexpr Vector2i cardinal_dirs[] = {
                { 0, 1 }, { 0, -1 }, { -1, 0 }, { 1, 0 }
            };
            for (const auto& dir : cardinal_dirs) {
                int nx = pos.x + dir.x;
                int ny = pos.y + dir.y;

                if (nx < 0 || ny < 0 || nx >= static_cast<int>(data.width)
                    || ny >= static_cast<int>(data.height))
                    continue;

                const Cell& neighbor = data.at(nx, ny);
                if (neighbor.material_type == MaterialType::DIRT
                    || neighbor.material_type == MaterialType::SAND) {
                    // ROOT grips soil - grant support.
                    cell.has_any_support = true;
                    if (GridOfCells::USE_CACHE) {
                        grid.supportBitmap().set(pos.x, pos.y);
                    }
                    LoggingChannels::tree()->debug(
                        "TreeManager: ROOT at ({},{}) anchored by {} at ({},{})",
                        pos.x,
                        pos.y,
                        getMaterialName(neighbor.material_type),
                        nx,
                        ny);
                    break;
                }
            }
        }

        // Step 2.8: Grant support to DIRT adjacent to supported ROOTs (soil reinforcement).
        for (const auto& pos : tree.cells) {
            if (static_cast<uint32_t>(pos.x) >= data.width
                || static_cast<uint32_t>(pos.y) >= data.height)
                continue;

            const Cell& root_cell = data.at(pos.x, pos.y);
            if (root_cell.organism_id != tree_id || root_cell.material_type != MaterialType::ROOT)
                continue;
            if (!root_cell.has_any_support) continue; // Root not anchored.

            // Grant support to adjacent dirt (root reinforces soil).
            static constexpr Vector2i cardinal_dirs[] = {
                { 0, 1 }, { 0, -1 }, { -1, 0 }, { 1, 0 }
            };
            for (const auto& dir : cardinal_dirs) {
                int nx = pos.x + dir.x;
                int ny = pos.y + dir.y;

                if (nx < 0 || ny < 0 || nx >= static_cast<int>(data.width)
                    || ny >= static_cast<int>(data.height))
                    continue;

                Cell& neighbor = data.at(nx, ny);
                if (neighbor.material_type == MaterialType::DIRT
                    || neighbor.material_type == MaterialType::SAND) {
                    // Reinforced by root - grant support.
                    neighbor.has_any_support = true;
                    if (GridOfCells::USE_CACHE) {
                        grid.supportBitmap().set(nx, ny);
                    }
                    LoggingChannels::tree()->debug(
                        "TreeManager: {} at ({},{}) reinforced by ROOT at ({},{})",
                        getMaterialName(neighbor.material_type),
                        nx,
                        ny,
                        pos.x,
                        pos.y);
                }
            }
        }

        // Step 3: Distribute support.
        // NOTE: Organism support only GRANTS additional support to cells that lack it.
        // We never remove support that cells already have from main physics (ground, cohesion).

        if (support_budget >= upper_mass) {
            // Roots can support entire tree - grant organism support to all unsupported cells.
            for (const auto& pos : tree.cells) {
                if (static_cast<uint32_t>(pos.x) >= data.width
                    || static_cast<uint32_t>(pos.y) >= data.height)
                    continue;
                Cell& cell = data.at(pos.x, pos.y);
                if (cell.organism_id == tree_id && !cell.has_any_support) {
                    // Grant organism support (update both cell flag and bitmap).
                    cell.has_any_support = true;
                    if (GridOfCells::USE_CACHE) {
                        grid.supportBitmap().set(pos.x, pos.y);
                    }
                }
            }

            LoggingChannels::tree()->debug(
                "TreeManager: Tree {} fully supported (budget={:.2f} >= mass={:.2f}, roots={})",
                tree_id,
                support_budget,
                upper_mass,
                root_count);
        }
        else {
            // Insufficient support - only grant organism support up to budget.
            double mass_to_support = support_budget; // How much we CAN support.

            LoggingChannels::tree()->warn(
                "TreeManager: Tree {} INSUFFICIENT support (budget={:.2f} < mass={:.2f})",
                tree_id,
                support_budget,
                upper_mass);

            // Grant organism support to random unsupported cells up to our budget.
            double mass_supported = 0.0;

            // Only consider cells that DON'T already have support.
            std::vector<Vector2i> unsupported_cells;
            for (const auto& pos : upper_cells) {
                Cell& cell = data.at(pos.x, pos.y);
                if (!cell.has_any_support) {
                    unsupported_cells.push_back(pos);
                }
            }

            // Shuffle for random selection.
            std::random_device rd;
            std::mt19937 rng(rd());
            std::shuffle(unsupported_cells.begin(), unsupported_cells.end(), rng);

            for (const auto& pos : unsupported_cells) {
                if (mass_supported >= mass_to_support) break;

                Cell& cell = data.at(pos.x, pos.y);
                const MaterialProperties& props = getMaterialProperties(cell.material_type);
                double cell_mass = cell.fill_ratio * props.density;

                // Grant organism support to this cell (update both cell flag and bitmap).
                cell.has_any_support = true;
                if (GridOfCells::USE_CACHE) {
                    grid.supportBitmap().set(pos.x, pos.y);
                }
                mass_supported += cell_mass;

                LoggingChannels::tree()->debug(
                    "TreeManager: Tree {} granting support to {} at ({}, {}) - mass={:.2f}",
                    tree_id,
                    getMaterialName(cell.material_type),
                    pos.x,
                    pos.y,
                    cell_mass);
            }

            LoggingChannels::tree()->info(
                "TreeManager: Tree {} partial support - {:.2f}/{:.2f} mass supported by roots",
                tree_id,
                mass_supported,
                upper_mass);
        }
    }
}

void TreeManager::applyBoneForces(World& world, double /*deltaTime*/)
{
    WorldData& data = world.getData();
    GridOfCells& grid = world.getGrid();
    constexpr double BONE_FORCE_SCALE = 1.0;
    constexpr double BONE_DAMPING_SCALE = 1.0; // Damping along bone (stretching/compression).
    constexpr double MAX_BONE_FORCE = 0.5;     // Maximum force per bone to prevent yanking.

    // Clear bone force debug info for all organism cells.
    for (auto& [tree_id, tree] : trees_) {
        for (const auto& pos : tree.cells) {
            if (static_cast<uint32_t>(pos.x) < data.width
                && static_cast<uint32_t>(pos.y) < data.height) {
                grid.debugAt(pos.x, pos.y).accumulated_bone_force = {};
            }
        }
    }

    for (auto& [tree_id, tree] : trees_) {
        for (const Bone& bone : tree.bones) {
            if (static_cast<uint32_t>(bone.cell_a.x) >= data.width
                || static_cast<uint32_t>(bone.cell_a.y) >= data.height
                || static_cast<uint32_t>(bone.cell_b.x) >= data.width
                || static_cast<uint32_t>(bone.cell_b.y) >= data.height) {
                continue;
            }

            Cell& cell_a = data.at(bone.cell_a.x, bone.cell_a.y);
            Cell& cell_b = data.at(bone.cell_b.x, bone.cell_b.y);

            // Skip if either cell no longer belongs to this organism.
            if (cell_a.organism_id != tree_id || cell_b.organism_id != tree_id) {
                continue;
            }

            // World positions including COM offset.
            Vector2d pos_a = Vector2d(bone.cell_a.x, bone.cell_a.y) + cell_a.com * 0.5;
            Vector2d pos_b = Vector2d(bone.cell_b.x, bone.cell_b.y) + cell_b.com * 0.5;

            Vector2d delta = pos_b - pos_a;
            double current_dist = delta.magnitude();

            if (current_dist < 1e-6) continue;

            double error = current_dist - bone.rest_distance;
            Vector2d direction = delta / current_dist;

            // Spring force: F_spring = stiffness * error * direction.
            Vector2d spring_force = direction * error * bone.stiffness * BONE_FORCE_SCALE;

            // Damping force: oppose stretching along bone.
            Vector2d relative_velocity = cell_b.velocity - cell_a.velocity;
            double velocity_along_bone = relative_velocity.dot(direction);
            Vector2d damping_along =
                direction * velocity_along_bone * bone.stiffness * BONE_DAMPING_SCALE;

            // Apply spring + along-bone damping (symmetric - both cells).
            Vector2d symmetric_force = spring_force + damping_along;

            // Limit maximum bone force to prevent yanking on transfers.
            double force_mag = symmetric_force.magnitude();
            if (force_mag > MAX_BONE_FORCE) {
                symmetric_force = symmetric_force.normalize() * MAX_BONE_FORCE;
            }

            cell_a.addPendingForce(symmetric_force);
            cell_b.addPendingForce(symmetric_force * -1.0);

            // Store symmetric forces in debug info.
            grid.debugAt(bone.cell_a.x, bone.cell_a.y).accumulated_bone_force += symmetric_force;
            grid.debugAt(bone.cell_b.x, bone.cell_b.y).accumulated_bone_force +=
                symmetric_force * -1.0;

            // Hinge-point rotational damping (if configured).
            if (bone.hinge_end != HingeEnd::NONE && bone.rotational_damping != 0.0) {
                // Determine which cell is the hinge (pivot) and which rotates.
                bool a_is_hinge = (bone.hinge_end == HingeEnd::CELL_A);
                Cell& rotating_cell = a_is_hinge ? cell_b : cell_a;
                Vector2i rotating_pos = a_is_hinge ? bone.cell_b : bone.cell_a;

                // Radius vector from hinge to rotating cell.
                Vector2d radius = a_is_hinge ? delta : (delta * -1.0);

                // Tangent direction (perpendicular to radius, for rotation).
                Vector2d tangent = Vector2d(-radius.y, radius.x).normalize();

                // Tangential velocity (how fast rotating around hinge).
                double tangential_velocity = rotating_cell.velocity.dot(tangent);

                // Rotational damping opposes tangential motion.
                Vector2d rot_damping_force =
                    tangent * (-tangential_velocity) * bone.rotational_damping;

                // Apply to rotating cell only (hinge stays fixed).
                rotating_cell.addPendingForce(rot_damping_force);
                grid.debugAt(rotating_pos.x, rotating_pos.y).accumulated_bone_force +=
                    rot_damping_force;
            }
        }
    }
}

} // namespace DirtSim
