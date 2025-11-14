#pragma once

#include "core/Vector2i.h"
#include <array>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <variant>
#include <zpp_bits.h>

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
 * Record of an organism-owned material transfer for efficient tracking updates.
 */
struct OrganismTransfer {
    Vector2i from_pos;
    Vector2i to_pos;
    TreeId organism_id;
    double amount; // Amount transferred.
};

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

    // Custom zpp_bits serialization (all 12 fields).
    using serialize = zpp::bits::members<12>;
};

/**
 * ADL (Argument-Dependent Lookup) functions for nlohmann::json automatic conversion.
 */
inline void to_json(nlohmann::json& j, const GrowthStage& stage)
{
    j = static_cast<uint8_t>(stage);
}

inline void from_json(const nlohmann::json& j, GrowthStage& stage)
{
    stage = static_cast<GrowthStage>(j.get<uint8_t>());
}

inline void to_json(nlohmann::json& j, const TreeSensoryData& data)
{
    j = nlohmann::json{ { "material_histograms", data.material_histograms },
                        { "actual_width", data.actual_width },
                        { "actual_height", data.actual_height },
                        { "scale_factor", data.scale_factor },
                        { "world_offset", data.world_offset },
                        { "age", data.age },
                        { "stage", data.stage },
                        { "total_energy", data.total_energy },
                        { "total_water", data.total_water },
                        { "root_count", data.root_count },
                        { "leaf_count", data.leaf_count },
                        { "wood_count", data.wood_count } };
}

inline void from_json(const nlohmann::json& j, TreeSensoryData& data)
{
    j.at("material_histograms").get_to(data.material_histograms);
    j.at("actual_width").get_to(data.actual_width);
    j.at("actual_height").get_to(data.actual_height);
    j.at("scale_factor").get_to(data.scale_factor);
    j.at("world_offset").get_to(data.world_offset);
    j.at("age").get_to(data.age);
    j.at("stage").get_to(data.stage);
    j.at("total_energy").get_to(data.total_energy);
    j.at("total_water").get_to(data.total_water);
    j.at("root_count").get_to(data.root_count);
    j.at("leaf_count").get_to(data.leaf_count);
    j.at("wood_count").get_to(data.wood_count);
}

} // namespace DirtSim
