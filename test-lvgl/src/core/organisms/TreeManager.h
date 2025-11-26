#pragma once

#include "Tree.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace DirtSim {

struct OrganismTransfer {
    Vector2i from_pos;
    Vector2i to_pos;
    TreeId organism_id;
    double amount;
};

class World;

class TreeManager {
public:
    TreeManager() = default;

    void update(World& world, double deltaTime);
    TreeId plantSeed(World& world, uint32_t x, uint32_t y);
    void removeTree(TreeId id);

    Tree* getTree(TreeId id);
    const Tree* getTree(TreeId id) const;
    TreeId getTreeAtCell(const Vector2i& pos) const;

    const std::unordered_map<TreeId, Tree>& getTrees() const { return trees_; }

    void notifyTransfers(const std::vector<OrganismTransfer>& transfers);

    /**
     * Compute realistic organism support for all trees.
     *
     * Trees are supported if connected to anchor points (ground, seed).
     * Uses flood-fill to mark reachable cells as supported.
     * Disconnected branches will lose support and fall.
     *
     * Should be called after main support calculation.
     */
    void computeOrganismSupport(World& world);

private:
    std::unordered_map<TreeId, Tree> trees_;
    std::unordered_map<Vector2i, TreeId> cell_to_tree_;
    uint32_t next_tree_id_ = 1;
};

} // namespace DirtSim
