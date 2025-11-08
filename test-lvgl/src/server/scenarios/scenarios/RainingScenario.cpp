#include "core/MaterialType.h"
#include "server/scenarios/Scenario.h"
#include "core/World.h"
#include "server/scenarios/ScenarioRegistry.h"
#include "server/scenarios/ScenarioWorldEventGenerator.h"
#include "spdlog/spdlog.h"

using namespace DirtSim;
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

        // Initialize with default config.
        config_.rain_rate = 5.0;
        config_.puddle_floor = true;
    }
    
    const ScenarioMetadata& getMetadata() const override {
        return metadata_;
    }

    ScenarioConfig getConfig() const override {
        return config_;
    }

    void setConfig(const ScenarioConfig& newConfig) override {
        // Validate type and update.
        if (std::holds_alternative<RainingConfig>(newConfig)) {
            config_ = std::get<RainingConfig>(newConfig);
            spdlog::info("RainingScenario: Config updated");
        }
        else {
            spdlog::error("RainingScenario: Invalid config type provided");
        }
    }

    std::unique_ptr<WorldEventGenerator> createWorldEventGenerator() const override
    {
        auto setup = std::make_unique<ScenarioWorldEventGenerator>();

        // Setup function - configure world size if possible
        setup->setSetupFunction([](World& world) {
            spdlog::info("Setting up Raining scenario");
            // Note: Current interface doesn't support resize, so we work with whatever size we have
            // In the future, we might want to add resize capability to WorldInterface
            world.setWallsEnabled(false);
            world.setLeftThrowEnabled(false);
            world.setRightThrowEnabled(false);
            world.setLowerRightQuadrantEnabled(false);
            // Gravity should already be set, but ensure it's on
            world.data.gravity = 9.81;
        });
        
        // Update function - add rain drops
        setup->setUpdateFunction([](World& world, uint32_t /*timestep*/, double deltaTime) {
            static std::mt19937 rng(42); // Deterministic for consistency
            static std::uniform_real_distribution<double> drop_dist(0.0, 1.0);
            static std::uniform_int_distribution<int> x_dist(1, world.data.width - 2);
            
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
    RainingConfig config_;
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