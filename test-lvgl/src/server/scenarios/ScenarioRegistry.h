#pragma once

#include "Scenario.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * Central registry for all available scenarios.
 * Owned by StateMachine to provide isolated registries for testing.
 */
class ScenarioRegistry {
public:
    ScenarioRegistry() = default;

    /**
     * @brief Create a registry populated with all available scenarios.
     * @return Initialized registry with all scenarios.
     */
    static ScenarioRegistry createDefault();

    // Register a scenario with the given ID
    void registerScenario(const std::string& id, std::unique_ptr<Scenario> scenario);

    // Get a scenario by ID
    Scenario* getScenario(const std::string& id) const;

    // Get all registered scenario IDs
    std::vector<std::string> getScenarioIds() const;

    // Get scenarios filtered by world type compatibility
    std::vector<std::string> getScenariosForWorldType(bool isWorldB) const;

    // Get scenarios filtered by category
    std::vector<std::string> getScenariosByCategory(const std::string& category) const;

    // Clear all registered scenarios (mainly for testing)
    void clear();

private:
    // Storage for scenarios
    std::unordered_map<std::string, std::unique_ptr<Scenario>> scenarios_;
};