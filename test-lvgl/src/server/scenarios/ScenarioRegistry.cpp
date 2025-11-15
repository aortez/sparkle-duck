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
#include "scenarios/TreeGerminationScenario.cpp"
#include "scenarios/WaterEqualizationScenario.cpp"

ScenarioRegistry ScenarioRegistry::createDefault()
{
    ScenarioRegistry registry;

    // Register all scenarios with metadata and factory functions.
    // Each registration creates a lambda that produces fresh instances.

    {
        auto temp = std::make_unique<DamBreakScenario>();
        registry.registerScenario("dam_break", temp->getMetadata(), []() {
            return std::make_unique<DamBreakScenario>();
        });
    }

    {
        auto temp = std::make_unique<EmptyScenario>();
        registry.registerScenario(
            "empty", temp->getMetadata(), []() { return std::make_unique<EmptyScenario>(); });
    }

    {
        auto temp = std::make_unique<FallingDirtScenario>();
        registry.registerScenario("falling_dirt", temp->getMetadata(), []() {
            return std::make_unique<FallingDirtScenario>();
        });
    }

    {
        auto temp = std::make_unique<RainingScenario>();
        registry.registerScenario(
            "raining", temp->getMetadata(), []() { return std::make_unique<RainingScenario>(); });
    }

    {
        auto temp = std::make_unique<SandboxScenario>();
        registry.registerScenario(
            "sandbox", temp->getMetadata(), []() { return std::make_unique<SandboxScenario>(); });
    }

    {
        auto temp = std::make_unique<TreeGerminationScenario>();
        registry.registerScenario("tree_germination", temp->getMetadata(), []() {
            return std::make_unique<TreeGerminationScenario>();
        });
    }

    {
        auto temp = std::make_unique<WaterEqualizationScenario>();
        registry.registerScenario("water_equalization", temp->getMetadata(), []() {
            return std::make_unique<WaterEqualizationScenario>();
        });
    }

    return registry;
}

void ScenarioRegistry::registerScenario(
    const std::string& id, const ScenarioMetadata& metadata, ScenarioFactory factory)
{
    if (!factory) {
        spdlog::error("Attempted to register null factory for scenario ID: {}", id);
        return;
    }

    if (scenarios_.find(id) != scenarios_.end()) {
        spdlog::warn("Scenario with ID '{}' already registered, overwriting", id);
    }

    spdlog::info("Registering scenario '{}' - {}", id, metadata.name);
    scenarios_[id] = ScenarioEntry{ metadata, std::move(factory) };
}

std::unique_ptr<Scenario> ScenarioRegistry::createScenario(const std::string& id) const
{
    auto it = scenarios_.find(id);
    if (it != scenarios_.end()) {
        return it->second.factory();
    }
    spdlog::error("Scenario '{}' not found in registry", id);
    return nullptr;
}

const ScenarioMetadata* ScenarioRegistry::getMetadata(const std::string& id) const
{
    auto it = scenarios_.find(id);
    if (it != scenarios_.end()) {
        return &it->second.metadata;
    }
    return nullptr;
}

std::vector<std::string> ScenarioRegistry::getScenarioIds() const
{
    std::vector<std::string> ids;
    ids.reserve(scenarios_.size());

    for (const auto& [id, entry] : scenarios_) {
        ids.push_back(id);
    }

    // Sort alphabetically for consistent UI display.
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<std::string> ScenarioRegistry::getScenariosByCategory(const std::string& category) const
{
    std::vector<std::string> ids;

    for (const auto& [id, entry] : scenarios_) {
        if (entry.metadata.category == category) {
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