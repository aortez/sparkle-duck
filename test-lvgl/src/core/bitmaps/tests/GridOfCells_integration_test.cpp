#include "core/GridOfCells.h"
#include "core/World.h"
#include "core/WorldData.h"

#include <chrono>
#include <gtest/gtest.h>
#include <random>
#include <spdlog/spdlog.h>

using namespace DirtSim;

/**
 * Integration test: Verify that GridOfCells cache produces identical results
 * to direct cell access across a full simulation run.
 */
TEST(GridOfCellsIntegrationTest, CacheProducesIdenticalResults)
{
    // Helper to run deterministic simulation.
    auto runSimulation = [](bool use_cache) -> nlohmann::json {
        // Set cache mode.
        GridOfCells::USE_CACHE = use_cache;

        // Create deterministic 10×10 world.
        World world(10, 10);
        world.setRandomSeed(42); // Deterministic RNG.

        // Populate with deterministic random materials (fixed seed).
        std::mt19937 rng(42);
        std::uniform_int_distribution<> coord_dist(1, 8); // Avoid walls.
        std::uniform_int_distribution<> mat_dist(1, 5);   // Material types.
        std::uniform_real_distribution<> fill_dist(0.3, 1.0);

        for (int i = 0; i < 15; ++i) {
            int x = coord_dist(rng);
            int y = coord_dist(rng);
            MaterialType mat = static_cast<MaterialType>(mat_dist(rng));
            double fill = fill_dist(rng);
            world.addMaterialAtCell(x, y, mat, fill);
        }

        // Run simulation for 100 frames.
        for (int frame = 0; frame < 100; ++frame) {
            world.advanceTime(0.016); // 60 FPS.
        }

        // Return serialized state.
        return world.toJSON();
    };

    auto removeHasSupport = [](nlohmann::json& state) {
        if (state.contains("cells") && state["cells"].is_array()) {
            for (auto& cell : state["cells"]) {
                if (cell.contains("has_support")) {
                    cell.erase("has_support");
                }
            }
        }
    };

    spdlog::info("Case 1: Running without cache (baseline)...");
    nlohmann::json case1_no_cache = runSimulation(false);
    removeHasSupport(case1_no_cache);
    size_t hash1 = std::hash<std::string>{}(case1_no_cache.dump());

    spdlog::info("Case 2: Running with cache...");
    nlohmann::json case2_with_cache = runSimulation(true);
    removeHasSupport(case2_with_cache);
    size_t hash2 = std::hash<std::string>{}(case2_with_cache.dump());

    spdlog::info("Case 3: Running without cache again (control)...");
    nlohmann::json case3_no_cache = runSimulation(false);
    removeHasSupport(case3_no_cache);
    size_t hash3 = std::hash<std::string>{}(case3_no_cache.dump());

    // Log all hashes.
    spdlog::info("Hash 1 (no cache):   {}", hash1);
    spdlog::info("Hash 2 (with cache): {}", hash2);
    spdlog::info("Hash 3 (no cache):   {}", hash3);

    // Verify determinism: Cases 1 and 3 should match.
    EXPECT_EQ(case1_no_cache, case3_no_cache)
        << "Control test failed: Simulation is non-deterministic!\n"
        << "Cases 1 and 3 (both without cache) produced different results.";

    // Verify cache correctness: Case 2 should match Case 1.
    EXPECT_EQ(case1_no_cache, case2_with_cache)
        << "Cache test failed: Cached path differs from direct path!\n"
        << "This indicates a bug in GridOfCells bitmap implementation.";

    // Restore default.
    GridOfCells::USE_CACHE = true;
}

/**
 * Simple single-frame test to isolate divergence.
 */
TEST(GridOfCellsIntegrationTest, SingleFrameComparison)
{
    auto runSingleFrame = [](bool use_cache) -> nlohmann::json {
        GridOfCells::USE_CACHE = use_cache;

        World world(10, 10);
        world.setRandomSeed(42); // Deterministic RNG.

        // Add one dirt cell.
        world.addMaterialAtCell(5, 5, MaterialType::DIRT, 1.0);

        // Run one frame.
        world.advanceTime(0.016);

        return world.toJSON();
    };

    nlohmann::json cached = runSingleFrame(true);
    nlohmann::json direct = runSingleFrame(false);

    if (cached != direct) {
        spdlog::error("DIVERGENCE on first frame!");
        spdlog::error("Cached cell(5,5): {}", cached["cells"][55].dump());
        spdlog::error("Direct cell(5,5): {}", direct["cells"][55].dump());
    }

    EXPECT_EQ(cached, direct) << "Results differ after single frame!";

    GridOfCells::USE_CACHE = true;
}

/**
 * Unit test: Verify GridOfCells bitmap accurately reflects cell emptiness.
 */
TEST(GridOfCellsTest, EmptyCellBitmapMatchesCellState)
{
    World world(20, 20);

    // Add some materials at known locations.
    world.addMaterialAtCell(5, 5, MaterialType::DIRT, 1.0);
    world.addMaterialAtCell(10, 10, MaterialType::WATER, 0.5);
    world.addMaterialAtCell(15, 15, MaterialType::METAL, 0.8);

    // Build grid cache.
    GridOfCells grid(
        world.getData().cells,
        world.getData().debug_info,
        world.getData().width,
        world.getData().height);

    // Verify every cell's bitmap state matches actual cell state.
    int mismatches = 0;
    for (uint32_t y = 0; y < 20; ++y) {
        for (uint32_t x = 0; x < 20; ++x) {
            bool bitmap_says_empty = grid.emptyCells().isSet(x, y);
            bool cell_is_empty = world.getData().at(x, y).isEmpty();

            if (bitmap_says_empty != cell_is_empty) {
                ++mismatches;
                EXPECT_EQ(bitmap_says_empty, cell_is_empty)
                    << "Mismatch at (" << x << "," << y << "): " << "bitmap=" << bitmap_says_empty
                    << " cell=" << cell_is_empty;
            }
        }
    }

    EXPECT_EQ(mismatches, 0) << "Found " << mismatches << " bitmap/cell mismatches";
}

/**
 * Performance comparison test: Measure overhead of cache construction.
 */
TEST(GridOfCellsTest, CacheConstructionOverhead)
{
    // Create a larger world for meaningful timing.
    World world(100, 100);

    // Populate with some materials.
    std::mt19937 rng(123);
    std::uniform_int_distribution<> coord_dist(0, 99);
    std::uniform_int_distribution<> mat_dist(1, 5);

    for (int i = 0; i < 500; ++i) {
        int x = coord_dist(rng);
        int y = coord_dist(rng);
        MaterialType mat = static_cast<MaterialType>(mat_dist(rng));
        world.addMaterialAtCell(x, y, mat, 0.5);
    }

    // Measure cache construction time.
    auto start = std::chrono::high_resolution_clock::now();
    GridOfCells grid(
        world.getData().cells,
        world.getData().debug_info,
        world.getData().width,
        world.getData().height);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    spdlog::info("GridOfCells construction (100x100): {} μs", duration.count());

    EXPECT_LT(duration.count(), 5000) << "Cache construction too slow!";
}
