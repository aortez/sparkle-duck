#include "../Scenario.h"
#include "../ScenarioWorldSetup.h"
#include "../ScenarioRegistry.h"
#include "../../WorldInterface.h"
#include "../../CellInterface.h"
#include "../../MaterialType.h"
#include "spdlog/spdlog.h"

/**
 * Dam Break scenario - Classic fluid dynamics demonstration.
 * Water held by a wall dam that breaks after pressure builds up.
 */
class DamBreakWorldSetup : public ScenarioWorldSetup {
private:
    bool damBroken = false;
    
public:
    void setup(WorldInterface& world) override {
        spdlog::info("Setting up Dam Break scenario");
        
        // Reset state
        damBroken = false;
        
        // Configure physics for dynamic pressure
        world.setGravity(9.81);
        world.setDynamicPressureEnabled(true);
        world.setHydrostaticPressureEnabled(false);
        world.setPressureDiffusionEnabled(true);
        world.setPressureScale(10.0); // Strong pressure for visible effects
        
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
    
    void addParticles(WorldInterface& world, uint32_t timestep, double /*deltaTimeSeconds*/) override {
        if (!damBroken && timestep == 30) {
            spdlog::info("Breaking the dam at timestep {}", timestep);
            
            // Dam is at x=2, break only the bottom cell for realistic flow
            world.getCellInterface(2, 5).clear();  // Bottom cell at (2,5)
            spdlog::info("Dam broken at (2, 5)");
            damBroken = true;
        }
    }
};

class DamBreakScenario : public Scenario {
public:
    DamBreakScenario() {
        metadata_.name = "Dam Break";
        metadata_.description = "Water column held by wall dam that breaks at timestep 30";
        metadata_.category = "demo";
        metadata_.supportsWorldA = false;  // Uses pressure systems
        metadata_.supportsWorldB = true;   // Primary target
        metadata_.requiredWidth = 6;       // Match test specifications
        metadata_.requiredHeight = 6;      // Match test specifications
    }
    
    const ScenarioMetadata& getMetadata() const override {
        return metadata_;
    }
    
    std::unique_ptr<WorldSetup> createWorldSetup() const override {
        return std::make_unique<DamBreakWorldSetup>();
    }
    
private:
    ScenarioMetadata metadata_;
};

// Self-registering scenario
namespace {
    struct DamBreakScenarioRegistrar {
        DamBreakScenarioRegistrar() {
            ScenarioRegistry::getInstance().registerScenario(
                "dam_break", 
                std::make_unique<DamBreakScenario>()
            );
        }
    } dam_break_scenario_registrar;
}