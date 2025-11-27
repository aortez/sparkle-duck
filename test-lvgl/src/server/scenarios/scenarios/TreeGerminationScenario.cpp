#include "core/Cell.h"
#include "core/MaterialType.h"
#include "core/World.h"
#include "core/WorldData.h"
#include "core/organisms/TreeManager.h"
#include "server/scenarios/Scenario.h"
#include "server/scenarios/ScenarioRegistry.h"
#include <spdlog/spdlog.h>

using namespace DirtSim;

class TreeGerminationScenario : public Scenario {
public:
    TreeGerminationScenario()
    {
        metadata_.name = "Tree Germination";
        metadata_.description = "9x9 world with seed growing into balanced tree";
        metadata_.category = "organisms";
        metadata_.requiredWidth = 9;
        metadata_.requiredHeight = 9;
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
        spdlog::info(
            "TreeGerminationScenario::setup - creating 9x9 world with balanced tree growth");

        // Clear world to air.
        for (uint32_t y = 0; y < world.getData().height; ++y) {
            for (uint32_t x = 0; x < world.getData().width; ++x) {
                world.getData().at(x, y) = Cell();
            }
        }

        // Dirt at bottom 3 rows.
        for (uint32_t y = 6; y < world.getData().height; ++y) {
            for (uint32_t x = 0; x < world.getData().width; ++x) {
                world.addMaterialAtCell(x, y, MaterialType::DIRT, 1.0);
            }
        }

        // Plant seed in center for balanced growth demonstration.
        TreeId tree_id = world.getTreeManager().plantSeed(world, 4, 4);
        spdlog::info("TreeGerminationScenario: Planted seed {} at (4, 4)", tree_id);
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
