#include "core/MaterialType.h"
#include "core/World.h"
#include "server/scenarios/Scenario.h"
#include "server/scenarios/ScenarioRegistry.h"
#include "server/scenarios/ScenarioWorldEventGenerator.h"
#include "spdlog/spdlog.h"
#include <random>

using namespace DirtSim;

/**
 * Falling Dirt scenario - Dirt particles falling and accumulating.
 */
class FallingDirtScenario : public Scenario {
public:
    FallingDirtScenario()
    {
        metadata_.name = "Falling Dirt";
        metadata_.description = "Dirt particles falling from the sky and accumulating";
        metadata_.category = "demo";

        // Initialize with default config.
        config_.drop_height = 20.0;
        config_.drop_rate = 2.0;
    }

    const ScenarioMetadata& getMetadata() const override { return metadata_; }

    ScenarioConfig getConfig() const override { return config_; }

    void setConfig(const ScenarioConfig& newConfig, World& /*world*/) override
    {
        // Validate type and update.
        if (std::holds_alternative<FallingDirtConfig>(newConfig)) {
            config_ = std::get<FallingDirtConfig>(newConfig);
            spdlog::info("FallingDirtScenario: Config updated");
        }
        else {
            spdlog::error("FallingDirtScenario: Invalid config type provided");
        }
    }

    std::unique_ptr<WorldEventGenerator> createWorldEventGenerator() const override
    {
        auto setup = std::make_unique<ScenarioWorldEventGenerator>();

        // Setup function - configure world
        setup->setSetupFunction([](World& world) {
            spdlog::info("Setting up Falling Dirt scenario");

            world.physicsSettings.gravity = 9.81;
            world.setWallsEnabled(false);
            world.setLeftThrowEnabled(false);
            world.setRightThrowEnabled(false);
            world.setLowerRightQuadrantEnabled(false);

            // Add some initial dirt piles to make it interesting
            uint32_t width = world.data.width;
            uint32_t height = world.data.height;

            // Create small dirt mounds at the bottom
            if (width >= 7 && height >= 7) {
                // Left mound
                world.addMaterialAtCell(1, height - 1, MaterialType::DIRT, 1.0);
                world.addMaterialAtCell(2, height - 1, MaterialType::DIRT, 1.0);
                world.addMaterialAtCell(1, height - 2, MaterialType::DIRT, 0.5);

                // Right mound
                world.addMaterialAtCell(width - 3, height - 1, MaterialType::DIRT, 1.0);
                world.addMaterialAtCell(width - 2, height - 1, MaterialType::DIRT, 1.0);
                world.addMaterialAtCell(width - 2, height - 2, MaterialType::DIRT, 0.5);
            }
        });

        // Update function - drop dirt particles
        setup->setUpdateFunction([](World& world, uint32_t /*timestep*/, double deltaTime) {
            static std::mt19937 rng(123); // Different seed than rain
            static std::uniform_real_distribution<double> drop_dist(0.0, 1.0);
            static std::uniform_int_distribution<int> x_dist(1, world.data.width - 2);

            // Dirt fall rate: particles per second
            const double dirt_rate = 5.0; // 5 dirt particles per second
            const double drop_probability = dirt_rate * deltaTime;

            // Add dirt particles based on probability
            if (drop_dist(rng) < drop_probability) {
                int x = x_dist(rng);
                uint32_t y = 1; // Start near top

                // Add dirt at the position
                world.addMaterialAtCell(x, y, MaterialType::DIRT, 0.7);
            }
        });

        return setup;
    }

private:
    ScenarioMetadata metadata_;
    FallingDirtConfig config_;
};
