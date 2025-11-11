#include "TreeManager.h"
#include "brains/RuleBasedBrain.h"
#include "core/MaterialType.h"
#include "core/World.h"
#include <spdlog/spdlog.h>

namespace DirtSim {

void TreeManager::update(World& world, double deltaTime)
{
    // Update all trees.
    for (auto& [id, tree] : trees_) {
        tree.update(world, deltaTime);
    }

    // TODO: Phase 3 - Update light map and process photosynthesis.
}

TreeId TreeManager::plantSeed(World& world, uint32_t x, uint32_t y)
{
    // Allocate new tree ID.
    TreeId id = next_tree_id_++;

    // Create tree with rule-based brain.
    auto brain = std::make_unique<RuleBasedBrain>();
    Tree tree(id, std::move(brain));

    // Place SEED material at position.
    world.addMaterialAtCell(x, y, MaterialType::SEED, 1.0);

    // Register seed cell with tree.
    Vector2i pos{ static_cast<int>(x), static_cast<int>(y) };
    tree.cells.insert(pos);
    cell_to_tree_[pos] = id;

    // Mark cell as owned by this tree.
    world.at(x, y).organism_id = id;

    spdlog::info("TreeManager: Planted seed for tree {} at ({}, {})", id, x, y);

    // Store tree.
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

} // namespace DirtSim
