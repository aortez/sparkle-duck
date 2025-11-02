#include "../../../core/MaterialType.h"
#include "../../../core/WorldInterface.h"
#include "../Scenario.h"
#include "../ScenarioRegistry.h"
#include "../ScenarioWorldEventGenerator.h"
#include "spdlog/spdlog.h"
#include <random>

/**
 * Raining scenario - Just rain falling from the sky.
 */
class RainingScenario : public Scenario {
public:
    RainingScenario() {
        metadata_.name = "Raining";
        metadata_.description = "Rain falling from the sky in a 50x50 world";
        metadata_.category = "demo";
        metadata_.supportsWorldA = true;
        metadata_.supportsWorldB = true;
    }
    
    const ScenarioMetadata& getMetadata() const override {
        return metadata_;
    }

    std::unique_ptr<WorldEventGenerator> createWorldEventGenerator() const override
    {
        auto setup = std::make_unique<ScenarioWorldEventGenerator>();

        // Setup function - configure world size if possible
        setup->setSetupFunction([](WorldInterface& world) {
            spdlog::info("Setting up Raining scenario");
            // Note: Current interface doesn't support resize, so we work with whatever size we have
            // In the future, we might want to add resize capability to WorldInterface
            world.setWallsEnabled(false);
            world.setLeftThrowEnabled(false);
            world.setRightThrowEnabled(false);
            world.setLowerRightQuadrantEnabled(false);
            // Gravity should already be set, but ensure it's on
            world.setGravity(9.81);
        });
        
        // Update function - add rain drops
        setup->setUpdateFunction([](WorldInterface& world, uint32_t /*timestep*/, double deltaTime) {
            static std::mt19937 rng(42); // Deterministic for consistency
            static std::uniform_real_distribution<double> drop_dist(0.0, 1.0);
            static std::uniform_int_distribution<int> x_dist(1, world.getWidth() - 2);
            
            // Rain rate: drops per second
            const double rain_rate = 10.0; // 10 drops per second
            const double drop_probability = rain_rate * deltaTime;
            
            // Add rain drops based on probability
            if (drop_dist(rng) < drop_probability) {
                int x = x_dist(rng);
                int y = 1; // Start near top
                
                // Add water at the position
                world.addMaterialAtCell(x, y, MaterialType::WATER, 0.5);
            }
        });
        
        return setup;
    }

private:
    ScenarioMetadata metadata_;
};

// Self-registering scenario
namespace {
    struct RainingScenarioRegistrar {
        RainingScenarioRegistrar() {
            ScenarioRegistry::getInstance().registerScenario(
                "raining", 
                std::make_unique<RainingScenario>()
            );
        }
    } raining_scenario_registrar;
}