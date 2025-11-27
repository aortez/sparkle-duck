#pragma once

#include "core/Vector2i.h"
#include <array>
#include <nlohmann/json.hpp>
#include <zpp_bits.h>

namespace DirtSim {

enum class GrowthStage : uint8_t { SEED, GERMINATION, SAPLING, MATURE, DECLINE };

struct TreeSensoryData {
    static constexpr int GRID_SIZE = 15;
    static constexpr int NUM_MATERIALS = 10;

    std::array<std::array<std::array<double, NUM_MATERIALS>, GRID_SIZE>, GRID_SIZE>
        material_histograms = {}; // Initialize to zeros.

    int actual_width = 0;
    int actual_height = 0;
    double scale_factor = 1.0;
    Vector2i world_offset;
    Vector2i seed_position;

    double age_seconds = 0.0;
    GrowthStage stage = GrowthStage::SEED;
    double total_energy = 0.0;
    double total_water = 0.0;
    std::string current_thought;

    using serialize = zpp::bits::members<11>;
};

void to_json(nlohmann::json& j, const GrowthStage& stage);
void from_json(const nlohmann::json& j, GrowthStage& stage);

void to_json(nlohmann::json& j, const TreeSensoryData& data);
void from_json(const nlohmann::json& j, TreeSensoryData& data);

} // namespace DirtSim
