#include "../Scenario.h"
#include "../../../core/World.h"
#include "../ScenarioRegistry.h"
#include "../ScenarioWorldEventGenerator.h"
#include "spdlog/spdlog.h"

/**
 * Empty scenario - A truly empty world with no particles.
 */
class EmptyScenario : public Scenario {
public:
    EmptyScenario() {
        metadata_.name = "Empty";
        metadata_.description = "A completely empty world with no particles";
        metadata_.category = "sandbox";
        metadata_.supportsWorldA = true;
        metadata_.supportsWorldB = true;
    }
    
    const ScenarioMetadata& getMetadata() const override {
        return metadata_;
    }

    std::unique_ptr<WorldEventGenerator> createWorldEventGenerator() const override
    {
        auto setup = std::make_unique<ScenarioWorldEventGenerator>();

        // Setup function - just clear the world
        setup->setSetupFunction([](World& /*world*/) {
            spdlog::info("Setting up Empty scenario");
            // reset() is called before setup(), so world is already empty
            // No additional setup needed
        });
        
        // Update function - no particles added
        setup->setUpdateFunction([](World& /*world*/, uint32_t /*timestep*/, double /*deltaTime*/) {
            // Intentionally empty - no particles added
        });
        
        return setup;
    }

private:
    ScenarioMetadata metadata_;
};

// Self-registering scenario
namespace {
    struct EmptyScenarioRegistrar {
        EmptyScenarioRegistrar() {
            ScenarioRegistry::getInstance().registerScenario(
                "empty", 
                std::make_unique<EmptyScenario>()
            );
        }
    } empty_scenario_registrar;
}