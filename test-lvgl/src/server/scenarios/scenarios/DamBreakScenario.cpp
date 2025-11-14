#include "core/MaterialType.h"
#include "core/World.h"
#include "server/scenarios/Scenario.h"
#include "server/scenarios/ScenarioRegistry.h"
#include "server/scenarios/ScenarioWorldEventGenerator.h"
#include "spdlog/spdlog.h"

using namespace DirtSim;

/**
 * Dam Break scenario - Classic fluid dynamics demonstration.
 * Water held by a wall dam that breaks after pressure builds up.
 */
class DamBreakWorldEventGenerator : public ScenarioWorldEventGenerator {
private:
    bool damBroken = false;

public:
    void setup(World& world) override
    {
        spdlog::info("Setting up Dam Break scenario");

        // Reset state
        damBroken = false;

        // Configure physics for dynamic pressure
        world.physicsSettings.gravity = 9.81;
        world.physicsSettings.pressure_dynamic_enabled = true;
        world.physicsSettings.pressure_dynamic_strength = 1.0;
        world.physicsSettings.pressure_hydrostatic_enabled = false;
        world.physicsSettings.pressure_hydrostatic_strength = 0.0;
        world.physicsSettings.pressure_diffusion_strength = 1.0;
        world.physicsSettings.pressure_scale = 1.0;

        // Disable extra features for clean demo
        world.setWallsEnabled(false);
        world.setLeftThrowEnabled(false);
        world.setRightThrowEnabled(false);
        world.setLowerRightQuadrantEnabled(false);

        // Create water column on left side - full height
        // Matches test case: water columns at x=0,1
        for (int x = 0; x < 2; x++) {
            for (int y = 0; y < 6; y++) {
                world.addMaterialAtCell(x, y, MaterialType::WATER, 1.0);
            }
        }

        // Create dam (temporary wall) at x=2 - full height using WALL
        for (int y = 0; y < 6; y++) {
            world.addMaterialAtCell(2, y, MaterialType::WALL, 1.0);
        }

        spdlog::info("Dam Break setup complete: 6x6 world, water columns at x=0,1, dam at x=2");
    }

    void addParticles(World& world, uint32_t timestep, double /*deltaTimeSeconds*/) override
    {
        if (!damBroken && timestep == 30) {
            spdlog::info("Breaking the dam at timestep {}", timestep);

            // Dam is at x=2, break only the bottom cell for realistic flow
            world.at(2, 5).clear(); // Bottom cell at (2,5)
            spdlog::info("Dam broken at (2, 5)");
            damBroken = true;
        }
    }
};

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
        config_.auto_release = false;
        config_.release_time = 2.0;
    }

    const ScenarioMetadata& getMetadata() const override { return metadata_; }

    ScenarioConfig getConfig() const override { return config_; }

    void setConfig(const ScenarioConfig& newConfig) override
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

    std::unique_ptr<WorldEventGenerator> createWorldEventGenerator() const override
    {
        return std::make_unique<DamBreakWorldEventGenerator>();
    }

private:
    ScenarioMetadata metadata_;
    DamBreakConfig config_;
};
