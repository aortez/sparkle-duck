#include "../Scenario.h"
#include "../ScenarioWorldSetup.h"
#include "../ScenarioRegistry.h"
#include "../../WorldInterface.h"
#include "../../CellInterface.h"
#include "../../MaterialType.h"
#include "spdlog/spdlog.h"

/**
 * Water Equalization scenario - Demonstrates hydrostatic pressure and flow.
 * Water flows through a small opening to achieve equilibrium between two columns.
 */
class WaterEqualizationWorldSetup : public ScenarioWorldSetup {
private:
    bool wallOpened = false;
    
public:
    void setup(WorldInterface& world) override {
        spdlog::info("Setting up Water Equalization scenario");
        
        // Reset state
        wallOpened = false;
        
        // Configure physics for hydrostatic pressure
        world.setGravity(9.81);
        world.setDynamicPressureEnabled(false);
        world.setHydrostaticPressureEnabled(true);
        world.setPressureDiffusionEnabled(true);
        world.setPressureScale(10.0); // Strong pressure for visible flow
        
        // Disable extra features for clean demo
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
    
    void addParticles(WorldInterface& world, uint32_t timestep, double /*deltaTimeSeconds*/) override {
        if (!wallOpened && timestep == 30) {
            spdlog::info("Opening wall at timestep {}", timestep);
            
            // Open bottom of middle wall at (1, 5)
            world.getCellInterface(1, 5).clear();
            spdlog::info("Wall opened at (1, 5)");
            wallOpened = true;
        }
        
        // Water equalization happens automatically through physics
    }
};

class WaterEqualizationScenario : public Scenario {
public:
    WaterEqualizationScenario() {
        metadata_.name = "Water Equalization";
        metadata_.description = "Water flows through opening to equalize between columns";
        metadata_.category = "demo";
        metadata_.supportsWorldA = false;  // Uses pressure systems
        metadata_.supportsWorldB = true;   // Primary target
        metadata_.requiredWidth = 3;       // Match test specifications
        metadata_.requiredHeight = 6;      // Match test specifications
    }
    
    const ScenarioMetadata& getMetadata() const override {
        return metadata_;
    }
    
    std::unique_ptr<WorldSetup> createWorldSetup() const override {
        return std::make_unique<WaterEqualizationWorldSetup>();
    }
    
private:
    ScenarioMetadata metadata_;
};

// Self-registering scenario
namespace {
    struct WaterEqualizationScenarioRegistrar {
        WaterEqualizationScenarioRegistrar() {
            ScenarioRegistry::getInstance().registerScenario(
                "water_equalization", 
                std::make_unique<WaterEqualizationScenario>()
            );
        }
    } water_equalization_scenario_registrar;
}