#pragma once

#include "../../core/WorldEventGenerator.h"
#include <functional>
#include <memory>

/**
 * A WorldEventGenerator implementation that wraps functional callbacks.
 * This allows scenarios to be defined using lambdas or function pointers
 * rather than requiring full class implementations.
 */
class ScenarioWorldEventGenerator : public WorldEventGenerator {
public:
    using SetupFunction = std::function<void(World&)>;
    using UpdateFunction = std::function<void(World&, uint32_t, double)>;
    using ResetFunction = std::function<void(World&)>;

    ScenarioWorldEventGenerator() = default;

    // Constructor with just setup function
    explicit ScenarioWorldEventGenerator(SetupFunction setupFn) : setup_fn_(std::move(setupFn)) {}

    // Constructor with setup and update functions
    ScenarioWorldEventGenerator(SetupFunction setupFn, UpdateFunction updateFn)
        : setup_fn_(std::move(setupFn)), update_fn_(std::move(updateFn))
    {}

    // Full constructor with all functions
    ScenarioWorldEventGenerator(
        SetupFunction setupFn, UpdateFunction updateFn, ResetFunction resetFn)
        : setup_fn_(std::move(setupFn)),
          update_fn_(std::move(updateFn)),
          reset_fn_(std::move(resetFn))
    {}

    ~ScenarioWorldEventGenerator() override = default;

    std::unique_ptr<WorldEventGenerator> clone() const override
    {
        auto cloned = std::make_unique<ScenarioWorldEventGenerator>();
        cloned->setup_fn_ = setup_fn_;
        cloned->update_fn_ = update_fn_;
        cloned->reset_fn_ = reset_fn_;
        return cloned;
    }

    // WorldEventGenerator interface implementation
    void setup(World& world) override
    {
        if (setup_fn_) {
            setup_fn_(world);
        }
    }

    void addParticles(World& world, uint32_t timestep, double deltaTimeSeconds) override
    {
        if (update_fn_) {
            update_fn_(world, timestep, deltaTimeSeconds);
        }
    }

    // Additional reset functionality
    void reset(World& world)
    {
        if (reset_fn_) {
            reset_fn_(world);
        }
        else if (setup_fn_) {
            // Default reset behavior: just call setup again
            setup_fn_(world);
        }
    }

    // Setters for individual functions
    void setSetupFunction(SetupFunction fn) { setup_fn_ = std::move(fn); }
    void setUpdateFunction(UpdateFunction fn) { update_fn_ = std::move(fn); }
    void setResetFunction(ResetFunction fn) { reset_fn_ = std::move(fn); }

private:
    SetupFunction setup_fn_;
    UpdateFunction update_fn_;
    ResetFunction reset_fn_;
};