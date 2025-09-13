#pragma once

#include "../WorldSetup.h"
#include <functional>
#include <memory>

/**
 * A WorldSetup implementation that wraps functional callbacks.
 * This allows scenarios to be defined using lambdas or function pointers
 * rather than requiring full class implementations.
 */
class ScenarioWorldSetup : public WorldSetup {
public:
    using SetupFunction = std::function<void(WorldInterface&)>;
    using UpdateFunction = std::function<void(WorldInterface&, uint32_t, double)>;
    using ResetFunction = std::function<void(WorldInterface&)>;
    
    ScenarioWorldSetup() = default;
    
    // Constructor with just setup function
    explicit ScenarioWorldSetup(SetupFunction setupFn)
        : setup_fn_(std::move(setupFn)) {}
    
    // Constructor with setup and update functions
    ScenarioWorldSetup(SetupFunction setupFn, UpdateFunction updateFn)
        : setup_fn_(std::move(setupFn)), update_fn_(std::move(updateFn)) {}
    
    // Full constructor with all functions
    ScenarioWorldSetup(SetupFunction setupFn, UpdateFunction updateFn, ResetFunction resetFn)
        : setup_fn_(std::move(setupFn)), update_fn_(std::move(updateFn)), reset_fn_(std::move(resetFn)) {}
    
    ~ScenarioWorldSetup() override = default;
    
    // WorldSetup interface implementation
    void setup(WorldInterface& world) override {
        if (setup_fn_) {
            setup_fn_(world);
        }
    }
    
    void addParticles(WorldInterface& world, uint32_t timestep, double deltaTimeSeconds) override {
        if (update_fn_) {
            update_fn_(world, timestep, deltaTimeSeconds);
        }
    }
    
    // Additional reset functionality
    void reset(WorldInterface& world) {
        if (reset_fn_) {
            reset_fn_(world);
        } else if (setup_fn_) {
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