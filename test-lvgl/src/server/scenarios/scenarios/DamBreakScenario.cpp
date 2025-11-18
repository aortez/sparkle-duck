#include "core/Cell.h"
#include "core/MaterialType.h"
#include "core/World.h"
#include "server/scenarios/Scenario.h"
#include "server/scenarios/ScenarioRegistry.h"
#include "spdlog/spdlog.h"

using namespace DirtSim;

/**
 * Dam Break scenario - Classic fluid dynamics demonstration.
 * Water held by a wall dam that breaks after pressure builds up.
 */
class DamBreakScenario : public Scenario {
public:
    DamBreakScenario()
    {
        metadata_.name = "Dam Break";
        metadata_.description = "Water column held by wall dam that breaks at timestep 30";
        metadata_.category = "demo";
        metadata_.requiredWidth = 6;  // Match test specifications
        metadata_.requiredHeight = 6; // Match test specifications

        // Initialize with default config.
        config_.dam_height = 10.0;
        config_.auto_release = true;
        config_.release_time = 2.0; // 2 seconds ~= timestep 30 at 60fps
    }

    const ScenarioMetadata& getMetadata() const override { return metadata_; }

    ScenarioConfig getConfig() const override { return config_; }

    void setConfig(const ScenarioConfig& newConfig, World& /*world*/) override
    {
        // Validate type and update.
        if (std::holds_alternative<DamBreakConfig>(newConfig)) {
            config_ = std::get<DamBreakConfig>(newConfig);
            spdlog::info("DamBreakScenario: Config updated");
        }
        else {
            spdlog::error("DamBreakScenario: Invalid config type provided");
        }
    }

    void setup(World& world) override
    {
        spdlog::info("DamBreakScenario::setup - initializing world");

        // Clear world first.
        for (uint32_t y = 0; y < world.data.height; ++y) {
            for (uint32_t x = 0; x < world.data.width; ++x) {
                world.at(x, y) = Cell(); // Reset to empty cell.
            }
        }

        // Reset state.
        damBroken_ = false;
        elapsedTime_ = 0.0;

        // Configure physics for dynamic pressure.
        world.physicsSettings.gravity = 9.81;
        world.physicsSettings.pressure_dynamic_enabled = true;
        world.physicsSettings.pressure_dynamic_strength = 1.0;
        world.physicsSettings.pressure_hydrostatic_enabled = false;
        world.physicsSettings.pressure_hydrostatic_strength = 0.0;
        world.physicsSettings.pressure_diffusion_strength = 1.0;
        world.physicsSettings.pressure_scale = 1.0;

        // Disable extra features for clean demo.
        world.setWallsEnabled(false);
        world.setLeftThrowEnabled(false);
        world.setRightThrowEnabled(false);
        world.setLowerRightQuadrantEnabled(false);

        // Create water column on left side - full height.
        for (uint32_t x = 0; x < 2; x++) {
            for (uint32_t y = 0; y < 6; y++) {
                world.addMaterialAtCell(x, y, MaterialType::WATER, 1.0);
            }
        }

        // Create dam (wall) at x=2 - full height.
        for (uint32_t y = 0; y < 6; y++) {
            world.addMaterialAtCell(2, y, MaterialType::WALL, 1.0);
        }

        spdlog::info("DamBreakScenario::setup complete - water at x=0-1, dam at x=2");
    }

    void reset(World& world) override
    {
        spdlog::info("DamBreakScenario::reset");
        setup(world);
    }

    void tick(World& world, double deltaTime) override
    {
        // Break dam automatically based on time if configured.
        if (!damBroken_ && config_.auto_release) {
            elapsedTime_ += deltaTime;

            if (elapsedTime_ >= config_.release_time) {
                spdlog::info("DamBreakScenario: Breaking dam at t={:.2f}s", elapsedTime_);

                // Dam is at x=2, break only the bottom cell for realistic flow.
                world.at(2, 5).clear(); // Bottom cell at (2,5)
                spdlog::info("DamBreakScenario: Dam broken at (2, 5)");
                damBroken_ = true;
            }
        }
    }

private:
    ScenarioMetadata metadata_;
    DamBreakConfig config_;

    // Scenario state.
    bool damBroken_ = false;
    double elapsedTime_ = 0.0;
};
