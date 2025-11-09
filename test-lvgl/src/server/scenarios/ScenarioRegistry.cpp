#include "ScenarioRegistry.h"
#include "core/ScenarioConfig.h"
#include "spdlog/spdlog.h"
#include <algorithm>

// Include scenario implementations.
#include "scenarios/DamBreakScenario.cpp"
#include "scenarios/EmptyScenario.cpp"
#include "scenarios/FallingDirtScenario.cpp"
#include "scenarios/RainingScenario.cpp"
#include "scenarios/SandboxScenario.cpp"
#include "scenarios/WaterEqualizationScenario.cpp"

ScenarioRegistry ScenarioRegistry::createDefault()
{
    ScenarioRegistry registry;

    // Register all scenarios.
    registry.registerScenario("empty", std::make_unique<EmptyScenario>());
    registry.registerScenario("sandbox", std::make_unique<SandboxScenario>());
    registry.registerScenario("dam_break", std::make_unique<DamBreakScenario>());
    registry.registerScenario("raining", std::make_unique<RainingScenario>());
    registry.registerScenario("water_equalization", std::make_unique<WaterEqualizationScenario>());
    registry.registerScenario("falling_dirt", std::make_unique<FallingDirtScenario>());

    return registry;
}

void ScenarioRegistry::registerScenario(const std::string& id, std::unique_ptr<Scenario> scenario)
{
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

Scenario* ScenarioRegistry::getScenario(const std::string& id) const
{
    auto it = scenarios_.find(id);
    if (it != scenarios_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<std::string> ScenarioRegistry::getScenarioIds() const
{
    std::vector<std::string> ids;
    ids.reserve(scenarios_.size());

    for (const auto& [id, scenario] : scenarios_) {
        ids.push_back(id);
    }

    // Sort alphabetically for consistent UI display
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<std::string> ScenarioRegistry::getScenariosForWorldType(bool isWorldB) const
{
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

std::vector<std::string> ScenarioRegistry::getScenariosByCategory(const std::string& category) const
{
    std::vector<std::string> ids;

    for (const auto& [id, scenario] : scenarios_) {
        if (scenario->getMetadata().category == category) {
            ids.push_back(id);
        }
    }

    std::sort(ids.begin(), ids.end());
    return ids;
}

void ScenarioRegistry::clear()
{
    spdlog::info("Clearing scenario registry");
    scenarios_.clear();
}