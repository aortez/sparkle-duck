#pragma once

#include <string>
#include <memory>

// Forward declarations
namespace DirtSim {
class World;
class WorldEventGenerator;
}

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
    
    // Compatibility flags
    bool supportsWorldA = true;  // Works with RulesA/World
    bool supportsWorldB = true;  // Works with RulesB/World
    
    // Required world dimensions (0 = any size is acceptable)
    uint32_t requiredWidth = 0;   // Required world width, 0 for flexible
    uint32_t requiredHeight = 0;  // Required world height, 0 for flexible
};

/**
 * Base interface for scenarios.
 * A scenario encapsulates a complete world configuration including:
 * - Initial setup (materials, parameters)
 * - Ongoing behavior (timed events, continuous effects)
 */
class Scenario {
public:
    virtual ~Scenario() = default;
    
    // Get metadata for UI display
    virtual const ScenarioMetadata& getMetadata() const = 0;

    // Create a WorldEventGenerator instance for this scenario
    virtual std::unique_ptr<WorldEventGenerator> createWorldEventGenerator() const = 0;
};