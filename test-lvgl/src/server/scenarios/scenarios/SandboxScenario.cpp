#include "core/WorldEventGenerator.h"
#include "server/scenarios/Scenario.h"
#include "core/World.h"
#include "server/scenarios/ScenarioRegistry.h"
#include "server/scenarios/ScenarioWorldEventGenerator.h"
#include "spdlog/spdlog.h"

using namespace DirtSim;

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

        // Initialize with default config.
        config_.quadrant_enabled = true;
        config_.water_column_enabled = true;
        config_.right_throw_enabled = true;
        config_.top_drop_enabled = true;
        config_.rain_rate = 0.0;
    }

    const ScenarioMetadata& getMetadata() const override {
        return metadata_;
    }

    ScenarioConfig getConfig() const override {
        return config_;
    }

    void setConfig(const ScenarioConfig& newConfig) override {
        // Validate type and update.
        if (std::holds_alternative<SandboxConfig>(newConfig)) {
            config_ = std::get<SandboxConfig>(newConfig);
            spdlog::info("SandboxScenario: Config updated");
        }
        else {
            spdlog::error("SandboxScenario: Invalid config type provided");
        }
    }

    std::unique_ptr<WorldEventGenerator> createWorldEventGenerator() const override
    {
        // Create a ConfigurableWorldEventGenerator using current config.
        auto configurableSetup = std::make_unique<ConfigurableWorldEventGenerator>();

        // Apply config settings to event generator.
        configurableSetup->setLowerRightQuadrantEnabled(config_.quadrant_enabled);
        configurableSetup->setWallsEnabled(true);  // Walls always enabled for physics containment.
        configurableSetup->setMiddleMetalWallEnabled(false);
        configurableSetup->setLeftThrowEnabled(false);
        configurableSetup->setRightThrowEnabled(config_.right_throw_enabled);
        configurableSetup->setTopDropEnabled(config_.top_drop_enabled);
        configurableSetup->setRainRate(config_.rain_rate);
        configurableSetup->setWaterColumnEnabled(config_.water_column_enabled);

        return configurableSetup;
    }

private:
    ScenarioMetadata metadata_;
    SandboxConfig config_;
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