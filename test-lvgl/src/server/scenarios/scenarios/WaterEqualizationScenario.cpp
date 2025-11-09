#include "core/MaterialType.h"
#include "core/World.h"
#include "server/scenarios/Scenario.h"
#include "server/scenarios/ScenarioRegistry.h"
#include "server/scenarios/ScenarioWorldEventGenerator.h"
#include "spdlog/spdlog.h"

using namespace DirtSim;

/**
 * Water Equalization scenario - Demonstrates hydrostatic pressure and flow.
 * Water flows through a small opening to achieve equilibrium between two columns.
 */
class WaterEqualizationWorldEventGenerator : public ScenarioWorldEventGenerator {
private:
    bool wallOpened = false;

public:
    void setup(World& world) override
    {
        spdlog::info("Setting up Water Equalization scenario");

        // Reset state
        wallOpened = false;

        world.physicsSettings.gravity = 9.81;
        world.setDynamicPressureEnabled(false);
        world.setHydrostaticPressureEnabled(true);
        world.setPressureDiffusionEnabled(true);
        world.physicsSettings.pressure_scale = 1.0;

        world.setWallsEnabled(false);
        world.setLeftThrowEnabled(false);
        world.setRightThrowEnabled(false);
        world.setLowerRightQuadrantEnabled(false);

        // 3x6 world with water on left, wall in middle, air on right
        // Left column (x=0): fill with water
        for (int y = 0; y < 6; y++) {
            world.addMaterialAtCell(0, y, MaterialType::WATER, 1.0);
        }

        // Middle column (x=1): wall barrier
        for (int y = 0; y < 6; y++) {
            world.addMaterialAtCell(1, y, MaterialType::WALL, 1.0);
        }

        // Right column (x=2): empty (air)
        // No need to explicitly set AIR

        spdlog::info("Water Equalization setup: 3x6 world, water at x=0, wall at x=1, air at x=2");
    }

    void addParticles(World& world, uint32_t timestep, double /*deltaTimeSeconds*/) override
    {
        if (!wallOpened && timestep == 30) {
            spdlog::info("Opening wall at timestep {}", timestep);

            // Open bottom of middle wall at (1, 5)
            world.at(1, 5).clear();
            spdlog::info("Wall opened at (1, 5)");
            wallOpened = true;
        }

        // Water equalization happens automatically through physics
    }
};

class WaterEqualizationScenario : public Scenario {
public:
    WaterEqualizationScenario()
    {
        metadata_.name = "Water Equalization";
        metadata_.description = "Water flows through opening to equalize between columns";
        metadata_.category = "demo";
        metadata_.supportsWorldA = false; // Uses pressure systems
        metadata_.supportsWorldB = true;  // Primary target
        metadata_.requiredWidth = 3;      // Match test specifications
        metadata_.requiredHeight = 6;     // Match test specifications

        // Initialize with default config.
        config_.left_height = 15.0;
        config_.right_height = 5.0;
        config_.separator_enabled = true;
    }

    const ScenarioMetadata& getMetadata() const override { return metadata_; }

    ScenarioConfig getConfig() const override { return config_; }

    void setConfig(const ScenarioConfig& newConfig) override
    {
        // Validate type and update.
        if (std::holds_alternative<WaterEqualizationConfig>(newConfig)) {
            config_ = std::get<WaterEqualizationConfig>(newConfig);
            spdlog::info("WaterEqualizationScenario: Config updated");
        }
        else {
            spdlog::error("WaterEqualizationScenario: Invalid config type provided");
        }
    }

    std::unique_ptr<WorldEventGenerator> createWorldEventGenerator() const override
    {
        return std::make_unique<WaterEqualizationWorldEventGenerator>();
    }

private:
    ScenarioMetadata metadata_;
    WaterEqualizationConfig config_;
};
