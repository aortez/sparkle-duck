#pragma once

#include "TreeBrain.h"
#include "TreeTypes.h"
#include <memory>
#include <optional>
#include <unordered_set>

namespace DirtSim {

// Forward declaration to avoid circular dependency.
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

    /**
     * Update tree state for one timestep.
     *
     * Executes current command, asks brain for next action if idle,
     * and updates resource tracking.
     *
     * @param world World reference for command execution.
     * @param deltaTime Time elapsed (unused, trees use discrete timesteps).
     */
    void update(World& world, double deltaTime);

    // Public members - simple data, no complex invariants to maintain.
    TreeId id;
    uint32_t age = 0;
    GrowthStage stage = GrowthStage::SEED;
    std::unordered_set<Vector2i> cells;         // Cell positions owned by this tree.
    double total_energy = 0.0;                  // Aggregated from world cells.
    double total_water = 0.0;                   // Aggregated from world cells.
    std::optional<TreeCommand> current_command; // Command being executed.
    uint32_t steps_remaining = 0;               // Timesteps until command completes.

private:
    std::unique_ptr<TreeBrain> brain_;

    /**
     * Execute the current command (called when steps_remaining reaches 0).
     */
    void executeCommand(World& world);

    /**
     * Ask brain for next action and enqueue it.
     */
    void decideNextAction(const World& world);

    /**
     * Update aggregated resources from world cells.
     */
    void updateResources(const World& world);

    /**
     * Gather scale-invariant sensory data for brain input.
     */
    TreeSensoryData gatherSensoryData(const World& world) const;
};

} // namespace DirtSim
