#include "../Scenario.h"
#include "../ScenarioWorldSetup.h"
#include "../ScenarioRegistry.h"
#include "../../WorldInterface.h"
#include "../../WorldSetup.h"
#include "spdlog/spdlog.h"

/**
 * Sandbox scenario - The default world setup without walls.
 */
class SandboxScenario : public Scenario {
public:
    SandboxScenario() {
        metadata_.name = "Sandbox";
        metadata_.description = "Default sandbox with dirt quadrant and particle streams (no walls)";
        metadata_.category = "sandbox";
        metadata_.supportsWorldA = true;
        metadata_.supportsWorldB = true;
    }
    
    const ScenarioMetadata& getMetadata() const override {
        return metadata_;
    }
    
    std::unique_ptr<WorldSetup> createWorldSetup() const override {
        // Create a ConfigurableWorldSetup that will persist with the world
        class SandboxConfigurableSetup : public ConfigurableWorldSetup {
        public:
            void setup(WorldInterface& world) override {
                // Only fill the lower right quadrant with dirt
                // Don't call makeWalls() - this is the sandbox behavior
                fillLowerRightQuadrant(world);
            }
        };
        
        auto configurableSetup = std::make_unique<SandboxConfigurableSetup>();
        
        // Configure initial settings
        configurableSetup->setLeftThrowEnabled(true);
        configurableSetup->setRightThrowEnabled(true);
        configurableSetup->setTopDropEnabled(true);
        configurableSetup->setRainRate(0.0); // No rain by default
        
        return configurableSetup;
    }
    
private:
    ScenarioMetadata metadata_;
};

// Self-registering scenario
namespace {
    struct SandboxScenarioRegistrar {
        SandboxScenarioRegistrar() {
            ScenarioRegistry::getInstance().registerScenario(
                "sandbox", 
                std::make_unique<SandboxScenario>()
            );
        }
    } sandbox_scenario_registrar;
}