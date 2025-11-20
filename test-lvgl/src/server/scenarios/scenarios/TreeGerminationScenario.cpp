#include "core/Cell.h"
#include "core/MaterialType.h"
#include "core/World.h"
#include "core/organisms/TreeManager.h"
#include "server/scenarios/Scenario.h"
#include "server/scenarios/ScenarioRegistry.h"
#include "spdlog/spdlog.h"

using namespace DirtSim;

class TreeGerminationScenario : public Scenario {
public:
    TreeGerminationScenario()
    {
        metadata_.name = "Tree Germination";
        metadata_.description = "7x7 world with falling seed";
        metadata_.category = "organisms";
        metadata_.requiredWidth = 7;
        metadata_.requiredHeight = 7;
    }

    const ScenarioMetadata& getMetadata() const override { return metadata_; }

    ScenarioConfig getConfig() const override { return config_; }

    void setConfig(const ScenarioConfig& newConfig, World& /*world*/) override
    {
        if (std::holds_alternative<EmptyConfig>(newConfig)) {
            config_ = std::get<EmptyConfig>(newConfig);
            spdlog::info("TreeGerminationScenario: Config updated");
        }
        else {
            spdlog::error("TreeGerminationScenario: Invalid config type provided");
        }
    }

    void setup(World& world) override
    {
        spdlog::info("TreeGerminationScenario::setup - creating 7x7 world");

        for (uint32_t y = 0; y < world.data.height; ++y) {
            for (uint32_t x = 0; x < world.data.width; ++x) {
                world.at(x, y) = Cell();
            }
        }

        for (uint32_t y = 4; y < world.data.height; ++y) {
            for (uint32_t x = 0; x < world.data.width; ++x) {
                world.addMaterialAtCell(x, y, MaterialType::DIRT, 1.0);
            }
        }

        TreeId tree_id = world.getTreeManager().plantSeed(world, 3, 1);
        spdlog::info("TreeGerminationScenario: Planted seed organism {} at (3, 1)", tree_id);
    }

    void reset(World& world) override
    {
        spdlog::info("TreeGerminationScenario::reset");
        setup(world);
    }

    void tick(World& /*world*/, double /*deltaTime*/) override
    {
        // No dynamic particles - just watch the tree grow.
    }

private:
    ScenarioMetadata metadata_;
    EmptyConfig config_; // No configuration needed.
};
