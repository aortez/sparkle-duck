#include "core/Cell.h"
#include "core/MaterialType.h"
#include "core/World.h"
#include "server/scenarios/Scenario.h"
#include "server/scenarios/ScenarioRegistry.h"
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
        config_.drop_rate = 5.0; // 5 particles per second
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

    void setup(World& world) override
    {
        spdlog::info("FallingDirtScenario::setup - initializing world");

        // Clear world first.
        for (uint32_t y = 0; y < world.data.height; ++y) {
            for (uint32_t x = 0; x < world.data.width; ++x) {
                world.at(x, y) = Cell(); // Reset to empty cell.
            }
        }

        // Configure physics.
        world.physicsSettings.gravity = 9.81;
        world.setWallsEnabled(false);
        world.setLeftThrowEnabled(false);
        world.setRightThrowEnabled(false);
        world.setLowerRightQuadrantEnabled(false);

        // Add floor.
        for (uint32_t x = 0; x < world.data.width; ++x) {
            world.at(x, world.data.height - 1).replaceMaterial(MaterialType::WALL, 1.0);
        }

        // Add some initial dirt piles to make it interesting.
        uint32_t width = world.data.width;
        uint32_t height = world.data.height;

        if (width >= 7 && height >= 7) {
            // Left mound
            world.addMaterialAtCell(1, height - 2, MaterialType::DIRT, 1.0);
            world.addMaterialAtCell(2, height - 2, MaterialType::DIRT, 1.0);
            world.addMaterialAtCell(1, height - 3, MaterialType::DIRT, 0.5);

            // Right mound
            world.addMaterialAtCell(width - 3, height - 2, MaterialType::DIRT, 1.0);
            world.addMaterialAtCell(width - 2, height - 2, MaterialType::DIRT, 1.0);
            world.addMaterialAtCell(width - 2, height - 3, MaterialType::DIRT, 0.5);
        }

        spdlog::info("FallingDirtScenario::setup complete");
    }

    void reset(World& world) override
    {
        spdlog::info("FallingDirtScenario::reset");
        setup(world);
    }

    void tick(World& world, double deltaTime) override
    {
        // Drop dirt particles based on configured rate.
        const double drop_probability = config_.drop_rate * deltaTime;

        if (drop_dist_(rng_) < drop_probability) {
            std::uniform_int_distribution<uint32_t> x_dist(1, world.data.width - 2);
            uint32_t x = x_dist(rng_);
            uint32_t y = 1; // Start near top.

            // Add dirt at the position.
            world.addMaterialAtCell(x, y, MaterialType::DIRT, 0.7);
        }
    }

private:
    ScenarioMetadata metadata_;
    FallingDirtConfig config_;

    // Random number generation for dirt drops.
    std::mt19937 rng_{ 123 }; // Different seed than rain
    std::uniform_real_distribution<double> drop_dist_{ 0.0, 1.0 };
};
