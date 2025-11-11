#pragma once

#include "Tree.h"
#include "TreeTypes.h"
#include <memory>
#include <unordered_map>

namespace DirtSim {

// Forward declaration.
class World;

/**
 * TreeManager manages all tree organisms in the simulation.
 *
 * Responsibilities:
 * - Tree lifecycle (planting seeds, germination, death).
 * - Cell ownership tracking (which tree owns which cell).
 * - Updating all trees each timestep.
 * - Resource systems (light, photosynthesis - Phase 3).
 */
class TreeManager {
public:
    TreeManager() = default;

    /**
     * Update all trees for one timestep.
     *
     * @param world World reference for tree updates.
     * @param deltaTime Time elapsed (trees use discrete timesteps).
     */
    void update(World& world, double deltaTime);

    /**
     * Plant a new tree seed at the specified position.
     *
     * @param world World reference for placing seed material.
     * @param x Cell x coordinate.
     * @param y Cell y coordinate.
     * @return TreeId of newly created tree.
     */
    TreeId plantSeed(World& world, uint32_t x, uint32_t y);

    /**
     * Remove a tree and all its cells.
     *
     * @param id Tree to remove.
     */
    void removeTree(TreeId id);

    /**
     * Get tree by ID (nullptr if not found).
     */
    Tree* getTree(TreeId id);
    const Tree* getTree(TreeId id) const;

    /**
     * Get tree ID owning a cell (INVALID_TREE_ID if none).
     */
    TreeId getTreeAtCell(const Vector2i& pos) const;

    /**
     * Get all trees.
     */
    const std::unordered_map<TreeId, Tree>& getTrees() const { return trees_; }

private:
    std::unordered_map<TreeId, Tree> trees_;
    std::unordered_map<Vector2i, TreeId> cell_to_tree_;
    uint32_t next_tree_id_ = 1; // Start at 1, 0 is INVALID_TREE_ID.

    // Phase 3: Resource systems.
    // std::vector<std::vector<float>> light_map_;
    // void updateLightMap(const World& world);
    // void processPhotosynthesis();
};

} // namespace DirtSim
