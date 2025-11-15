#include "core/Cell.h"
#include "core/World.h"
#include "server/scenarios/Scenario.h"
#include "server/scenarios/ScenarioRegistry.h"
#include "server/scenarios/ScenarioWorldEventGenerator.h"
#include "spdlog/spdlog.h"

using namespace DirtSim;

/**
 * Empty scenario - A truly empty world with no particles.
 */
class EmptyScenario : public Scenario {
public:
    EmptyScenario()
    {
        metadata_.name = "Empty";
        metadata_.description = "A completely empty world with no particles";
        metadata_.category = "sandbox";
    }

    const ScenarioMetadata& getMetadata() const override { return metadata_; }

    ScenarioConfig getConfig() const override { return config_; }

    void setConfig(const ScenarioConfig& newConfig) override
    {
        // Validate type and update.
        if (std::holds_alternative<EmptyConfig>(newConfig)) {
            config_ = std::get<EmptyConfig>(newConfig);
            spdlog::info("EmptyScenario: Config updated");
        }
        else {
            spdlog::error("EmptyScenario: Invalid config type provided");
        }
    }

    // DEPRECATED: Temporary compatibility - uses base class defaults.
    std::unique_ptr<WorldEventGenerator> createWorldEventGenerator() const override
    {
        auto setup = std::make_unique<ScenarioWorldEventGenerator>();
        setup->setSetupFunction(
            [](World& /*world*/) { spdlog::info("Setting up Empty scenario"); });
        setup->setUpdateFunction([](World& /*world*/, uint32_t /*timestep*/, double /*deltaTime*/) {
            // Intentionally empty - no particles added.
        });
        return setup;
    }

private:
    ScenarioMetadata metadata_;
    EmptyConfig config_;
};
