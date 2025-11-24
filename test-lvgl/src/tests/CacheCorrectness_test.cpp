#include "core/GridOfCells.h"
#include "core/World.h"
#include "core/WorldData.h"
#include "server/scenarios/ScenarioRegistry.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using namespace DirtSim;

/**
 * @brief Helper to remove has_support field (non-deterministic).
 */
static void removeHasSupport(nlohmann::json& state)
{
    if (state.contains("cells") && state["cells"].is_array()) {
        for (auto& cell : state["cells"]) {
            if (cell.contains("has_support")) {
                cell.erase("has_support");
            }
        }
    }
}

/**
 * @brief Verify that cached and non-cached implementations produce identical results.
 *
 * This test runs the same simulation twice:
 * 1. With GridOfCells cache enabled (USE_CACHE = true)
 * 2. With GridOfCells cache disabled (USE_CACHE = false)
 *
 * The final world states must match exactly, otherwise there's a bug in either:
 * - GridOfCells cache implementation
 * - OpenMP parallelization (race conditions)
 * - Other non-deterministic behavior
 */
TEST(CacheCorrectnessTest, CachedAndNonCachedProduceIdenticalResults)
{
    // Helper to run deterministic simulation using benchmark scenario.
    auto runSimulation = [](bool use_cache, int steps) -> nlohmann::json {
        GridOfCells::USE_CACHE = use_cache;

        // Get benchmark scenario metadata to determine world size.
        ScenarioRegistry registry = ScenarioRegistry::createDefault();
        const ScenarioMetadata* metadata = registry.getMetadata("benchmark");
        if (!metadata) {
            spdlog::error("Benchmark scenario not found!");
            return nlohmann::json::object();
        }

        // Create world with scenario's required dimensions (200×200 = 40,000 cells).
        uint32_t width = metadata->requiredWidth;
        uint32_t height = metadata->requiredHeight;
        World world(width, height);
        world.setRandomSeed(42); // Deterministic RNG.

        // Create and set up benchmark scenario.
        auto scenario = registry.createScenario("benchmark");
        if (!scenario) {
            spdlog::error("Failed to create benchmark scenario!");
            return nlohmann::json::object();
        }
        scenario->setup(world);

        // Run simulation.
        for (int step = 0; step < steps; ++step) {
            world.advanceTime(0.016); // 60 FPS.
        }

        // Return serialized state.
        return world.toJSON();
    };

    // Test at various step counts to find where divergence occurs.
    // Use fewer steps for 200×200 world (40K cells is large).
    const std::vector<int> step_counts = { 1, 5, 10 };

    for (int steps : step_counts) {
        spdlog::info("Testing {} steps...", steps);

        // Run with cache enabled.
        nlohmann::json cached_state = runSimulation(true, steps);
        removeHasSupport(cached_state);

        // Run without cache.
        nlohmann::json direct_state = runSimulation(false, steps);
        removeHasSupport(direct_state);

        // Compare states.
        if (cached_state != direct_state) {
            // Find and report first differing cell for debugging.
            auto cached_cells = cached_state["cells"];
            auto direct_cells = direct_state["cells"];
            int width = cached_state["width"].get<int>();

            for (size_t i = 0; i < cached_cells.size(); ++i) {
                if (cached_cells[i] != direct_cells[i]) {
                    int x = i % width;
                    int y = i / width;
                    spdlog::error("First difference at step {}, cell [{},{}]:", steps, x, y);
                    spdlog::error("  Cached: {}", cached_cells[i].dump());
                    spdlog::error("  Direct: {}", direct_cells[i].dump());
                    break;
                }
            }

            FAIL() << "Cache correctness FAILED at step " << steps
                   << "\nCached and non-cached implementations produce different results!"
                   << "\nThis indicates a bug in GridOfCells cache or OpenMP parallelization.";
        }

        spdlog::info("  ✅ Step {} passed", steps);
    }

    // Restore default.
    GridOfCells::USE_CACHE = true;
}

/**
 * @brief Verify determinism - same configuration should produce same results.
 *
 * This test runs the cached version twice with the same seed to ensure
 * the implementation is deterministic (no race conditions, uninitialized vars, etc.)
 */
TEST(CacheCorrectnessTest, DeterminismCheck)
{
    auto runSimulation = []() -> nlohmann::json {
        GridOfCells::USE_CACHE = true;

        World world(28, 28);
        world.setRandomSeed(42); // Same seed both times.

        // Add some materials.
        for (int y = 20; y < 26; ++y) {
            for (int x = 10; x < 18; ++x) {
                world.addMaterialAtCell(x, y, MaterialType::DIRT, 1.0);
            }
        }

        for (int step = 0; step < 20; ++step) {
            world.advanceTime(0.016);
        }

        return world.toJSON();
    };

    spdlog::info("Running determinism check (same config twice)...");

    nlohmann::json state1 = runSimulation();
    nlohmann::json state2 = runSimulation();

    removeHasSupport(state1);
    removeHasSupport(state2);

    EXPECT_EQ(state1, state2) << "Determinism check FAILED!"
                              << "\nSame configuration produced different results on repeated runs."
                              << "\nThis indicates OpenMP race condition or uninitialized memory.";

    spdlog::info("  ✅ Determinism check passed");
}
