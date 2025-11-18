#include "core/Cell.h"
#include "core/MaterialType.h"
#include "core/World.h"
#include "server/scenarios/Scenario.h"
#include "server/scenarios/ScenarioRegistry.h"
#include "spdlog/spdlog.h"

using namespace DirtSim;

/**
 * Water Equalization scenario - Demonstrates hydrostatic pressure and flow.
 * Water flows through a small opening at the bottom to achieve equilibrium between two columns.
 */
class WaterEqualizationScenario : public Scenario {
public:
    WaterEqualizationScenario()
    {
        metadata_.name = "Water Equalization";
        metadata_.description = "Water flows through bottom opening to equalize between columns";
        metadata_.category = "demo";
        metadata_.requiredWidth = 3;  // Match test specifications
        metadata_.requiredHeight = 6; // Match test specifications

        // Initialize with default config.
        config_.left_height = 15.0;
        config_.right_height = 5.0;
        config_.separator_enabled = true;
    }

    const ScenarioMetadata& getMetadata() const override { return metadata_; }

    ScenarioConfig getConfig() const override { return config_; }

    void setConfig(const ScenarioConfig& newConfig, World& /*world*/) override
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

    void setup(World& world) override
    {
        spdlog::info("WaterEqualizationScenario::setup - initializing world");

        // Clear world first.
        for (uint32_t y = 0; y < world.data.height; ++y) {
            for (uint32_t x = 0; x < world.data.width; ++x) {
                world.at(x, y) = Cell(); // Reset to empty cell.
            }
        }

        // Configure physics for hydrostatic pressure demonstration.
        world.physicsSettings.gravity = 9.81;
        world.physicsSettings.pressure_dynamic_enabled = false;
        world.physicsSettings.pressure_dynamic_strength = 0.0;
        world.physicsSettings.pressure_hydrostatic_enabled = true;
        world.physicsSettings.pressure_hydrostatic_strength = 0.3;
        world.physicsSettings.pressure_diffusion_strength = 1.0;
        world.physicsSettings.pressure_scale = 1.0;

        world.setWallsEnabled(false);
        world.setLeftThrowEnabled(false);
        world.setRightThrowEnabled(false);
        world.setLowerRightQuadrantEnabled(false);

        // 3x6 world with water on left, wall separator in middle, air on right.
        // Left column (x=0): fill with water.
        for (uint32_t y = 0; y < 6; y++) {
            world.addMaterialAtCell(0, y, MaterialType::WATER, 1.0);
        }

        // Middle column (x=1): wall barrier with bottom cell open for flow.
        for (uint32_t y = 0; y < 5; y++) { // Only y=0 to y=4 (leave y=5 open)
            world.addMaterialAtCell(1, y, MaterialType::WALL, 1.0);
        }
        // Bottom cell at (1, 5) is left empty for water to flow through.

        // Right column (x=2): empty (air) - no need to explicitly set.

        spdlog::info(
            "WaterEqualizationScenario::setup complete - water at x=0, wall at x=1 (y=0-4), bottom "
            "open at (1,5)");
    }

    void reset(World& world) override
    {
        spdlog::info("WaterEqualizationScenario::reset - resetting world");
        setup(world);
    }

    void tick(World& /*world*/, double /*deltaTime*/) override
    {
        // No dynamic particle generation needed.
        // Water equalization happens automatically through physics.
    }

private:
    ScenarioMetadata metadata_;
    WaterEqualizationConfig config_;
};
