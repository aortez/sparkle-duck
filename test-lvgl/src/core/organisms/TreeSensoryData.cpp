#include "TreeSensoryData.h"

namespace DirtSim {

void to_json(nlohmann::json& j, const GrowthStage& stage)
{
    j = static_cast<uint8_t>(stage);
}

void from_json(const nlohmann::json& j, GrowthStage& stage)
{
    stage = static_cast<GrowthStage>(j.get<uint8_t>());
}

void to_json(nlohmann::json& j, const TreeSensoryData& data)
{
    j = nlohmann::json{ { "material_histograms", data.material_histograms },
                        { "actual_width", data.actual_width },
                        { "actual_height", data.actual_height },
                        { "scale_factor", data.scale_factor },
                        { "world_offset", data.world_offset },
                        { "seed_position", data.seed_position },
                        { "age_seconds", data.age_seconds },
                        { "stage", data.stage },
                        { "total_energy", data.total_energy },
                        { "total_water", data.total_water },
                        { "current_thought", data.current_thought } };
}

void from_json(const nlohmann::json& j, TreeSensoryData& data)
{
    j.at("material_histograms").get_to(data.material_histograms);
    j.at("actual_width").get_to(data.actual_width);
    j.at("actual_height").get_to(data.actual_height);
    j.at("scale_factor").get_to(data.scale_factor);
    j.at("world_offset").get_to(data.world_offset);
    j.at("seed_position").get_to(data.seed_position);
    j.at("age_seconds").get_to(data.age_seconds);
    j.at("stage").get_to(data.stage);
    j.at("total_energy").get_to(data.total_energy);
    j.at("total_water").get_to(data.total_water);
    j.at("current_thought").get_to(data.current_thought);
}

} // namespace DirtSim
