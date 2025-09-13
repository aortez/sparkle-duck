#include "../Scenario.h"
#include "../ScenarioWorldSetup.h"
#include "../ScenarioRegistry.h"
#include "../../WorldInterface.h"
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
    
    std::unique_ptr<WorldSetup> createWorldSetup() const override {
        auto setup = std::make_unique<ScenarioWorldSetup>();
        
        // Setup function - just clear the world
        setup->setSetupFunction([](WorldInterface& /*world*/) {
            spdlog::info("Setting up Empty scenario");
            // reset() is called before setup(), so world is already empty
            // No additional setup needed
        });
        
        // Update function - no particles added
        setup->setUpdateFunction([](WorldInterface& /*world*/, uint32_t /*timestep*/, double /*deltaTime*/) {
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