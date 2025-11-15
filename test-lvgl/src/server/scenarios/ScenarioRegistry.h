#pragma once

#include "Scenario.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * Central registry for all available scenarios.
 * Uses factory pattern to create fresh scenario instances (not singletons).
 * Owned by StateMachine to provide isolated registries for testing.
 */
class ScenarioRegistry {
public:
    ScenarioRegistry() = default;

    /**
     * @brief Create a registry populated with all available scenarios.
     * @return Initialized registry with all scenario factories.
     */
    static ScenarioRegistry createDefault();

    // Register a scenario factory function with the given ID.
    using ScenarioFactory = std::function<std::unique_ptr<Scenario>()>;
    void registerScenario(
        const std::string& id, const ScenarioMetadata& metadata, ScenarioFactory factory);

    // Create a new scenario instance by ID (factory pattern).
    std::unique_ptr<Scenario> createScenario(const std::string& id) const;

    // Get metadata for a scenario by ID (no instance created).
    const ScenarioMetadata* getMetadata(const std::string& id) const;

    // Get all registered scenario IDs.
    std::vector<std::string> getScenarioIds() const;

    // Get scenarios filtered by category.
    std::vector<std::string> getScenariosByCategory(const std::string& category) const;

    // Clear all registered scenarios (mainly for testing).
    void clear();

private:
    // Storage for scenario metadata and factories.
    struct ScenarioEntry {
        ScenarioMetadata metadata;
        ScenarioFactory factory;
    };
    std::unordered_map<std::string, ScenarioEntry> scenarios_;
};