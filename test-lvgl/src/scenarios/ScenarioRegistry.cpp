#include "ScenarioRegistry.h"
#include "spdlog/spdlog.h"
#include <algorithm>

ScenarioRegistry& ScenarioRegistry::getInstance() {
    static ScenarioRegistry instance;
    return instance;
}

void ScenarioRegistry::registerScenario(const std::string& id, std::unique_ptr<Scenario> scenario) {
    if (!scenario) {
        spdlog::error("Attempted to register null scenario with ID: {}", id);
        return;
    }
    
    if (scenarios_.find(id) != scenarios_.end()) {
        spdlog::warn("Scenario with ID '{}' already registered, overwriting", id);
    }
    
    spdlog::info("Registering scenario '{}' - {}", id, scenario->getMetadata().name);
    scenarios_[id] = std::move(scenario);
}

Scenario* ScenarioRegistry::getScenario(const std::string& id) const {
    auto it = scenarios_.find(id);
    if (it != scenarios_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<std::string> ScenarioRegistry::getScenarioIds() const {
    std::vector<std::string> ids;
    ids.reserve(scenarios_.size());
    
    for (const auto& [id, scenario] : scenarios_) {
        ids.push_back(id);
    }
    
    // Sort alphabetically for consistent UI display
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<std::string> ScenarioRegistry::getScenariosForWorldType(bool isWorldB) const {
    std::vector<std::string> ids;
    
    for (const auto& [id, scenario] : scenarios_) {
        const auto& metadata = scenario->getMetadata();
        if ((isWorldB && metadata.supportsWorldB) || (!isWorldB && metadata.supportsWorldA)) {
            ids.push_back(id);
        }
    }
    
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<std::string> ScenarioRegistry::getScenariosByCategory(const std::string& category) const {
    std::vector<std::string> ids;
    
    for (const auto& [id, scenario] : scenarios_) {
        if (scenario->getMetadata().category == category) {
            ids.push_back(id);
        }
    }
    
    std::sort(ids.begin(), ids.end());
    return ids;
}

void ScenarioRegistry::clear() {
    spdlog::info("Clearing scenario registry");
    scenarios_.clear();
}