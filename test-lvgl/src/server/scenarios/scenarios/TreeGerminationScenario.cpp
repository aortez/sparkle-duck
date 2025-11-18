#include "core/Cell.h"
#include "core/MaterialType.h"
#include "core/World.h"
#include "core/organisms/TreeManager.h"
#include "server/scenarios/Scenario.h"
#include "server/scenarios/ScenarioRegistry.h"
#include "spdlog/spdlog.h"

using namespace DirtSim;

/**
 * Tree Germination Test Scenario - Small 5x5 world to observe seed germination.
 *
 * Layout (5 wide × 5 tall):
 * Row 0: ----- (AIR)
 * Row 1: ----- (AIR)
 * Row 2: wwsdd (WALL, WALL, SEED, DIRT, DIRT)
 * Row 3: wwddd (WALL, WALL, DIRT, DIRT, DIRT)
 * Row 4: ddddd (DIRT all across bottom)
 *
 * Seed at (2, 2) will germinate after 100 timesteps:
 * - t=100: SEED → WOOD (germination)
 * - t=120: ROOT grows at (2, 3)
 */
class TreeGerminationScenario : public Scenario {
public:
    TreeGerminationScenario()
    {
        metadata_.name = "Tree Germination";
        metadata_.description = "Small 5x5 test world with single seed organism";
        metadata_.category = "organisms";
        metadata_.requiredWidth = 5;
        metadata_.requiredHeight = 5;
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
        spdlog::info("TreeGerminationScenario::setup - creating 5x5 world");

        // Clear world first.
        for (uint32_t y = 0; y < world.data.height; ++y) {
            for (uint32_t x = 0; x < world.data.width; ++x) {
                world.at(x, y) = Cell(); // Reset to empty cell.
            }
        }

        // Row 0: ----- (AIR - already default).
        // Row 1: ----- (AIR - already default).

        // Row 2: wwsdd.
        world.addMaterialAtCell(0, 2, MaterialType::WALL, 1.0);
        world.addMaterialAtCell(1, 2, MaterialType::WALL, 1.0);
        // Seed at (2, 2) planted via TreeManager below.
        world.addMaterialAtCell(3, 2, MaterialType::DIRT, 1.0);
        world.addMaterialAtCell(4, 2, MaterialType::DIRT, 1.0);

        // Row 3: wwddd.
        world.addMaterialAtCell(0, 3, MaterialType::WALL, 1.0);
        world.addMaterialAtCell(1, 3, MaterialType::WALL, 1.0);
        world.addMaterialAtCell(2, 3, MaterialType::DIRT, 1.0);
        world.addMaterialAtCell(3, 3, MaterialType::DIRT, 1.0);
        world.addMaterialAtCell(4, 3, MaterialType::DIRT, 1.0);

        // Row 4: ddddd.
        world.addMaterialAtCell(0, 4, MaterialType::DIRT, 1.0);
        world.addMaterialAtCell(1, 4, MaterialType::DIRT, 1.0);
        world.addMaterialAtCell(2, 4, MaterialType::DIRT, 1.0);
        world.addMaterialAtCell(3, 4, MaterialType::DIRT, 1.0);
        world.addMaterialAtCell(4, 4, MaterialType::DIRT, 1.0);

        // Plant seed organism at (2, 2).
        TreeId tree_id = world.getTreeManager().plantSeed(world, 2, 2);
        spdlog::info("TreeGerminationScenario: Planted seed organism {} at (2, 2)", tree_id);
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
