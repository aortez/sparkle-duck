#pragma once

#include "Scenario.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * Central registry for all available scenarios.
 * Implemented as a singleton to provide global access from UI components.
 */
class ScenarioRegistry {
public:
    // Get the singleton instance
    static ScenarioRegistry& getInstance();
    
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
    // Private constructor for singleton
    ScenarioRegistry() = default;
    
    // Delete copy/move operations
    ScenarioRegistry(const ScenarioRegistry&) = delete;
    ScenarioRegistry& operator=(const ScenarioRegistry&) = delete;
    ScenarioRegistry(ScenarioRegistry&&) = delete;
    ScenarioRegistry& operator=(ScenarioRegistry&&) = delete;
    
    // Storage for scenarios
    std::unordered_map<std::string, std::unique_ptr<Scenario>> scenarios_;
};