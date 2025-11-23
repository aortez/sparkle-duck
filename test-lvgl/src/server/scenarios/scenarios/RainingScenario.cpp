#include "core/Cell.h"
#include "core/MaterialType.h"
#include "core/PhysicsSettings.h"
#include "core/World.h"
#include "core/WorldData.h"
#include "server/scenarios/Scenario.h"
#include "server/scenarios/ScenarioRegistry.h"
#include "spdlog/spdlog.h"
#include <random>

using namespace DirtSim;

/**
 * Raining scenario - Rain falling from the sky.
 */
class RainingScenario : public Scenario {
public:
    RainingScenario()
    {
        metadata_.name = "Raining";
        metadata_.description = "Rain falling from the sky";
        metadata_.category = "demo";

        // Initialize with default config.
        config_.rain_rate = 10.0; // 10 drops per second
        config_.puddle_floor = true;
    }

    const ScenarioMetadata& getMetadata() const override { return metadata_; }

    ScenarioConfig getConfig() const override { return config_; }

    void setConfig(const ScenarioConfig& newConfig, World& /*world*/) override
    {
        // Validate type and update.
        if (std::holds_alternative<RainingConfig>(newConfig)) {
            config_ = std::get<RainingConfig>(newConfig);
            spdlog::info("RainingScenario: Config updated");
        }
        else {
            spdlog::error("RainingScenario: Invalid config type provided");
        }
    }

    void setup(World& world) override
    {
        spdlog::info("RainingScenario::setup - initializing world");

        // Clear world first.
        for (uint32_t y = 0; y < world.getData().height; ++y) {
            for (uint32_t x = 0; x < world.getData().width; ++x) {
                world.getData().at(x, y) = Cell(); // Reset to empty cell.
            }
        }

        // Configure physics.
        world.setWallsEnabled(false);
        world.setLeftThrowEnabled(false);
        world.setRightThrowEnabled(false);
        world.setLowerRightQuadrantEnabled(false);
        world.getPhysicsSettings().gravity = 9.81;

        // Add floor if configured.
        if (config_.puddle_floor) {
            for (uint32_t x = 0; x < world.getData().width; ++x) {
                world.getData()
                    .at(x, world.getData().height - 1)
                    .replaceMaterial(MaterialType::WALL, 1.0);
            }
        }

        spdlog::info("RainingScenario::setup complete");
    }

    void reset(World& world) override
    {
        spdlog::info("RainingScenario::reset");
        setup(world);
    }

    void tick(World& world, double deltaTime) override
    {
        // Add rain drops based on configured rain rate.
        const double drop_probability = config_.rain_rate * deltaTime;

        if (drop_dist_(rng_) < drop_probability) {
            std::uniform_int_distribution<uint32_t> x_dist(1, world.getData().width - 2);
            uint32_t x = x_dist(rng_);
            uint32_t y = 1; // Start near top.

            // Add water at the position.
            world.addMaterialAtCell(x, y, MaterialType::WATER, 0.5);
        }
    }

private:
    ScenarioMetadata metadata_;
    RainingConfig config_;

    // Random number generation for rain drops.
    std::mt19937 rng_{ 42 }; // Deterministic seed for consistency
    std::uniform_real_distribution<double> drop_dist_{ 0.0, 1.0 };
};
