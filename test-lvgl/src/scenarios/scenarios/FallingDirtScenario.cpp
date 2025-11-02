#include "../../MaterialType.h"
#include "../../WorldInterface.h"
#include "../Scenario.h"
#include "../ScenarioRegistry.h"
#include "../ScenarioWorldEventGenerator.h"
#include "spdlog/spdlog.h"
#include <random>

/**
 * Falling Dirt scenario - Dirt particles falling and accumulating.
 */
class FallingDirtScenario : public Scenario {
public:
    FallingDirtScenario() {
        metadata_.name = "Falling Dirt";
        metadata_.description = "Dirt particles falling from the sky and accumulating";
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

        // Setup function - configure world
        setup->setSetupFunction([](WorldInterface& world) {
            spdlog::info("Setting up Falling Dirt scenario");
            
            world.setGravity(9.81);
            world.setWallsEnabled(false);
            world.setLeftThrowEnabled(false);
            world.setRightThrowEnabled(false);
            world.setLowerRightQuadrantEnabled(false);
            
            // Add some initial dirt piles to make it interesting
            uint32_t width = world.getWidth();
            uint32_t height = world.getHeight();
            
            // Create small dirt mounds at the bottom
            if (width >= 7 && height >= 7) {
                // Left mound
                world.addMaterialAtCell(1, height - 1, MaterialType::DIRT, 1.0);
                world.addMaterialAtCell(2, height - 1, MaterialType::DIRT, 1.0);
                world.addMaterialAtCell(1, height - 2, MaterialType::DIRT, 0.5);
                
                // Right mound
                world.addMaterialAtCell(width - 3, height - 1, MaterialType::DIRT, 1.0);
                world.addMaterialAtCell(width - 2, height - 1, MaterialType::DIRT, 1.0);
                world.addMaterialAtCell(width - 2, height - 2, MaterialType::DIRT, 0.5);
            }
        });
        
        // Update function - drop dirt particles
        setup->setUpdateFunction([](WorldInterface& world, uint32_t /*timestep*/, double deltaTime) {
            static std::mt19937 rng(123); // Different seed than rain
            static std::uniform_real_distribution<double> drop_dist(0.0, 1.0);
            static std::uniform_int_distribution<int> x_dist(1, world.getWidth() - 2);
            
            // Dirt fall rate: particles per second
            const double dirt_rate = 5.0; // 5 dirt particles per second
            const double drop_probability = dirt_rate * deltaTime;
            
            // Add dirt particles based on probability
            if (drop_dist(rng) < drop_probability) {
                int x = x_dist(rng);
                uint32_t y = 1; // Start near top
                
                // Add dirt at the position
                world.addMaterialAtCell(x, y, MaterialType::DIRT, 0.7);
            }
        });
        
        return setup;
    }

private:
    ScenarioMetadata metadata_;
};

// Self-registering scenario
namespace {
    struct FallingDirtScenarioRegistrar {
        FallingDirtScenarioRegistrar() {
            ScenarioRegistry::getInstance().registerScenario(
                "falling_dirt", 
                std::make_unique<FallingDirtScenario>()
            );
        }
    } falling_dirt_scenario_registrar;
}