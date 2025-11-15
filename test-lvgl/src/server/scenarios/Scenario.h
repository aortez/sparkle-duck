#pragma once

#include "core/ScenarioConfig.h"
#include "core/WorldEventGenerator.h"
#include <memory>
#include <string>

// Forward declarations.
namespace DirtSim {
class World;
} // namespace DirtSim

using namespace DirtSim;

/**
 * Core types for the scenario system.
 */

/**
 * Metadata for a scenario.
 */
struct ScenarioMetadata {
    std::string name;        // Display name
    std::string description; // Tooltip/help text
    std::string category;    // Organization category (test, demo, sandbox)

    // Required world dimensions (0 = any size is acceptable)
    uint32_t requiredWidth = 0;  // Required world width, 0 for flexible
    uint32_t requiredHeight = 0; // Required world height, 0 for flexible
};

/**
 * Base interface for scenarios.
 * A scenario encapsulates a complete world configuration and behavior:
 * - Initial setup (materials, parameters)
 * - Ongoing behavior (particle generation, timed events)
 * - Runtime-configurable parameters (via ScenarioConfig)
 * - State management (timers, counters specific to the scenario)
 *
 * Scenarios are instanced (not singletons) so each can maintain independent state.
 */
class Scenario {
protected:
    // Temporary: Delegate to old generator during migration.
    mutable std::unique_ptr<WorldEventGenerator> generator_;

public:
    virtual ~Scenario() = default;

    // Get metadata for UI display.
    virtual const ScenarioMetadata& getMetadata() const = 0;

    // Get current configuration for this scenario.
    virtual ScenarioConfig getConfig() const = 0;

    // Update configuration (UI can change settings at runtime).
    // Takes World reference to immediately apply config changes.
    virtual void setConfig(const ScenarioConfig& config, World& world) = 0;

    // Initialize world to scenario's starting state.
    // Default implementation delegates to old generator (migration path).
    virtual void setup(World& world);

    // Reset scenario state (timers, counters) and re-initialize world.
    // Default implementation recreates generator and calls setup.
    virtual void reset(World& world);

    // Update scenario behavior each frame (particle generation, timed events, etc.).
    // Default implementation delegates to old generator (migration path).
    virtual void tick(World& world, double deltaTime);

    // DEPRECATED: Will be removed after refactor.
    virtual std::unique_ptr<WorldEventGenerator> createWorldEventGenerator() const = 0;
};