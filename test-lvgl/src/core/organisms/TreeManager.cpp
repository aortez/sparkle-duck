#include "TreeManager.h"
#include "brains/RuleBasedBrain.h"
#include "core/MaterialType.h"
#include "core/World.h"
#include <spdlog/spdlog.h>

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
    tree.total_energy = 100.0;

    world.addMaterialAtCell(x, y, MaterialType::SEED, 1.0);

    tree.cells.insert(pos);
    cell_to_tree_[pos] = id;

    world.at(x, y).organism_id = id;

    spdlog::info("TreeManager: Planted seed for tree {} at ({}, {})", id, x, y);

    trees_.emplace(id, std::move(tree));

    return id;
}

void TreeManager::removeTree(TreeId id)
{
    auto it = trees_.find(id);
    if (it == trees_.end()) {
        spdlog::warn("TreeManager: Attempted to remove non-existent tree {}", id);
        return;
    }

    // Remove cell ownership tracking.
    for (const auto& pos : it->second.cells) {
        cell_to_tree_.erase(pos);
    }

    // Remove tree.
    trees_.erase(it);

    spdlog::info("TreeManager: Removed tree {}", id);
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
    // Batch transfers by tree ID for efficient processing.
    std::unordered_map<TreeId, std::vector<const OrganismTransfer*>> transfers_by_tree;

    for (const auto& transfer : transfers) {
        transfers_by_tree[transfer.organism_id].push_back(&transfer);
    }

    // Update each affected tree's cell tracking.
    for (const auto& [tree_id, tree_transfers] : transfers_by_tree) {
        auto tree_it = trees_.find(tree_id);
        if (tree_it == trees_.end()) {
            spdlog::warn("TreeManager: Received transfers for non-existent tree {}", tree_id);
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
                spdlog::debug(
                    "TreeManager: Tree {} seed moved from ({}, {}) to ({}, {})",
                    tree_id,
                    transfer->from_pos.x,
                    transfer->from_pos.y,
                    transfer->to_pos.x,
                    transfer->to_pos.y);
            }

            // Note: We don't remove from_pos yet - source cell might still have material.
            // The cleanup will happen in a separate pass or when cell becomes fully empty.
        }

        spdlog::trace(
            "TreeManager: Processed {} transfers for tree {} (now {} cells tracked)",
            tree_transfers.size(),
            tree_id,
            tree.cells.size());
    }
}

} // namespace DirtSim
