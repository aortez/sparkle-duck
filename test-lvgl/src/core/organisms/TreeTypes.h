#pragma once

#include "core/Vector2i.h"
#include <array>
#include <cstdint>
#include <variant>

namespace DirtSim {

/**
 * Unique identifier for tree organisms.
 */
using TreeId = uint32_t;

/**
 * Invalid tree ID sentinel value.
 */
constexpr TreeId INVALID_TREE_ID = 0;

/**
 * Growth stages for tree lifecycle.
 */
enum class GrowthStage : uint8_t {
    SEED,        // Dormant seed waiting for germination.
    GERMINATION, // Seed converting to wood, growing first root.
    SAPLING,     // Rapid growth phase, establishing structure.
    MATURE,      // Balanced growth, can produce seeds.
    DECLINE      // Resource shortage, cells dying.
};

/**
 * Tree command: Grow WOOD cell at target position.
 */
struct GrowWoodCommand {
    Vector2i target_pos;
    uint32_t execution_time = 50; // Timesteps.
    double energy_cost = 10.0;
};

/**
 * Tree command: Grow LEAF cell at target position.
 */
struct GrowLeafCommand {
    Vector2i target_pos;
    uint32_t execution_time = 30; // Timesteps.
    double energy_cost = 8.0;
};

/**
 * Tree command: Grow ROOT cell at target position.
 */
struct GrowRootCommand {
    Vector2i target_pos;
    uint32_t execution_time = 60; // Timesteps.
    double energy_cost = 12.0;
};

/**
 * Tree command: Reinforce existing cell (increase structural integrity).
 */
struct ReinforceCellCommand {
    Vector2i position;
    uint32_t execution_time = 30; // Timesteps.
    double energy_cost = 5.0;
};

/**
 * Tree command: Produce a seed at position.
 */
struct ProduceSeedCommand {
    Vector2i position;
    uint32_t execution_time = 100; // Timesteps.
    double energy_cost = 50.0;
};

/**
 * Tree command: Wait for duration (idle).
 */
struct WaitCommand {
    uint32_t duration = 10; // Timesteps.
};

/**
 * Variant of all possible tree commands.
 */
using TreeCommand = std::variant<
    GrowWoodCommand,
    GrowLeafCommand,
    GrowRootCommand,
    ReinforceCellCommand,
    ProduceSeedCommand,
    WaitCommand>;

/**
 * Scale-invariant sensory data for tree brains.
 *
 * Uses a fixed 15x15 neural grid regardless of actual tree size.
 * Small trees: native resolution, one-hot histograms.
 * Large trees: downsampled with material distribution histograms.
 */
struct TreeSensoryData {
    // Fixed-size neural grid (scale-invariant).
    static constexpr int GRID_SIZE = 15;
    static constexpr int NUM_MATERIALS =
        9; // AIR, DIRT, LEAF, METAL, SAND, SEED, WALL, WATER, WOOD.

    /**
     * Material distribution histograms for each neural cell.
     * Small trees: one-hot encoding [0,0,0,1,0,0,0,0,0].
     * Large trees: distributions [0.4,0.1,0,0.3,0,0,0.2,0,0].
     */
    std::array<std::array<std::array<double, NUM_MATERIALS>, GRID_SIZE>, GRID_SIZE>
        material_histograms;

    // Metadata about spatial mapping.
    int actual_width = 0; // Real bounding box size.
    int actual_height = 0;
    double scale_factor = 1.0; // Real cells per neural cell.
    Vector2i world_offset;     // Top-left corner in world coords.

    // Internal organism state.
    uint32_t age = 0;
    GrowthStage stage = GrowthStage::SEED;
    double total_energy = 0.0;
    double total_water = 0.0;
    int root_count = 0;
    int leaf_count = 0;
    int wood_count = 0;
};

} // namespace DirtSim
