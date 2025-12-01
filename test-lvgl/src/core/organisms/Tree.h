#pragma once

#include "TreeBrain.h"
#include "TreeCommands.h"
#include "TreeSensoryData.h"
#include "core/MaterialType.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

namespace DirtSim {

using TreeId = uint32_t;

constexpr TreeId INVALID_TREE_ID = 0;

struct Bone {
    Vector2i cell_a;
    Vector2i cell_b;
    double rest_distance;
    double stiffness;
};

double getBoneStiffness(MaterialType a, MaterialType b);

class World;

/**
 * Tree organism class.
 *
 * Trees are living organisms composed of physics cells (SEED, WOOD, LEAF, ROOT)
 * that participate fully in simulation while being coordinated by a brain.
 *
 * Trees execute commands over time, consume resources, and make growth decisions
 * through pluggable brain implementations.
 */
class Tree {
public:
    /**
     * Construct a new tree with a given brain implementation.
     *
     * @param id Unique tree identifier.
     * @param brain Brain implementation for decision making.
     */
    Tree(TreeId id, std::unique_ptr<TreeBrain> brain);

    // Move-only type (unique_ptr members).
    Tree(Tree&&) = default;
    Tree& operator=(Tree&&) = default;
    Tree(const Tree&) = delete;
    Tree& operator=(const Tree&) = delete;

    void update(World& world, double deltaTime);

    TreeId id;
    Vector2i seed_position;
    double age_seconds = 0.0;
    GrowthStage stage = GrowthStage::SEED;
    std::unordered_set<Vector2i> cells;
    std::vector<Bone> bones;
    double total_energy = 0.0;
    double total_water = 0.0;
    std::optional<TreeCommand> current_command;
    double time_remaining_seconds = 0.0;

    TreeSensoryData gatherSensoryData(const World& world) const;

    void createBonesForCell(Vector2i new_cell, MaterialType material, const World& world);

    // Replace the brain (for testing with custom brain implementations).
    void setBrain(std::unique_ptr<TreeBrain> brain) { brain_ = std::move(brain); }

private:
    std::unique_ptr<TreeBrain> brain_;

    void executeCommand(World& world);
    void decideNextAction(const World& world);
    void updateResources(const World& world);
};

} // namespace DirtSim
