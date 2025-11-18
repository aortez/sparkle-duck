#include "core/Cell.h"
#include "core/World.h"
#include "server/scenarios/Scenario.h"
#include "server/scenarios/ScenarioRegistry.h"
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

    void setConfig(const ScenarioConfig& newConfig, World& /*world*/) override
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

    void setup(World& world) override
    {
        spdlog::info("EmptyScenario::setup - clearing world");

        // Clear world to empty state.
        for (uint32_t y = 0; y < world.data.height; ++y) {
            for (uint32_t x = 0; x < world.data.width; ++x) {
                world.at(x, y) = Cell(); // Reset to empty cell.
            }
        }

        spdlog::info("EmptyScenario::setup complete");
    }

    void reset(World& world) override
    {
        spdlog::info("EmptyScenario::reset");
        setup(world);
    }

    void tick(World& /*world*/, double /*deltaTime*/) override
    {
        // Intentionally empty - no dynamic particles.
    }

private:
    ScenarioMetadata metadata_;
    EmptyConfig config_;
};
