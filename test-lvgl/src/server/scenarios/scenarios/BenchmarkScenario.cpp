#include "core/Cell.h"
#include "core/MaterialType.h"
#include "core/World.h"
#include "server/scenarios/Scenario.h"
#include "server/scenarios/ScenarioRegistry.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <cstdlib>

using namespace DirtSim;

/**
 * Benchmark scenario - Performance testing with complex physics.
 * 200x200 world with water pool and falling metal/wood balls.
 */
class BenchmarkScenario : public Scenario {
public:
    BenchmarkScenario()
    {
        metadata_.name = "Benchmark";
        metadata_.description = "Performance test: 200x200 world with water pool and falling balls";
        metadata_.category = "benchmark";
        metadata_.requiredWidth = 200;
        metadata_.requiredHeight = 200;

        // Initialize with empty config.
        config_ = BenchmarkConfig{};
    }

    const ScenarioMetadata& getMetadata() const override { return metadata_; }

    ScenarioConfig getConfig() const override { return config_; }

    void setConfig(const ScenarioConfig& newConfig, World& /*world*/) override
    {
        // Validate type and update.
        if (std::holds_alternative<BenchmarkConfig>(newConfig)) {
            config_ = std::get<BenchmarkConfig>(newConfig);
            spdlog::info("BenchmarkScenario: Config updated");
        }
        else {
            spdlog::error("BenchmarkScenario: Invalid config type provided");
        }
    }

    void setup(World& world) override;
    void reset(World& world) override;

    // No ongoing behavior needed - just initial setup.
    void tick(World& /*world*/, double /*deltaTime*/) override {}

private:
    ScenarioMetadata metadata_;
    BenchmarkConfig config_;

    void addBall(
        World& world, uint32_t centerX, uint32_t centerY, uint32_t radius, MaterialType material);
};

// ============================================================================
// BenchmarkScenario Implementation
// ============================================================================

void BenchmarkScenario::setup(World& world)
{
    spdlog::info(
        "BenchmarkScenario::setup - initializing {}x{} world", world.data.width, world.data.height);

    // Clear world first.
    for (uint32_t y = 0; y < world.data.height; ++y) {
        for (uint32_t x = 0; x < world.data.width; ++x) {
            world.at(x, y) = Cell(); // Reset to empty cell.
        }
    }

    // Create boundary walls.
    for (uint32_t x = 0; x < world.data.width; ++x) {
        world.at(x, 0).replaceMaterial(MaterialType::WALL, 1.0);                     // Top wall.
        world.at(x, world.data.height - 1).replaceMaterial(MaterialType::WALL, 1.0); // Bottom wall.
    }
    for (uint32_t y = 0; y < world.data.height; ++y) {
        world.at(0, y).replaceMaterial(MaterialType::WALL, 1.0);                    // Left wall.
        world.at(world.data.width - 1, y).replaceMaterial(MaterialType::WALL, 1.0); // Right wall.
    }

    // Fill bottom 1/3 with water.
    uint32_t waterStartY = world.data.height - (world.data.height / 3);
    for (uint32_t y = waterStartY; y < world.data.height - 1; ++y) {
        for (uint32_t x = 1; x < world.data.width - 1; ++x) {
            world.at(x, y).replaceMaterial(MaterialType::WATER, 1.0);
        }
    }
    spdlog::info("Added water pool (bottom 1/3): rows {}-{}", waterStartY, world.data.height - 1);

    // Calculate ball diameter as 15% of minimum world dimension.
    uint32_t minDimension = std::min(world.data.width, world.data.height);
    uint32_t ballDiameter = static_cast<uint32_t>(minDimension * 0.15);
    uint32_t ballRadius = ballDiameter / 2;

    // Position balls proportionally to world size.
    uint32_t metalBallX = world.data.width / 5;
    uint32_t metalBallY = world.data.height / 10;
    addBall(world, metalBallX, metalBallY, ballRadius, MaterialType::METAL);
    spdlog::info("Added metal ball at ({}, {}), radius {}", metalBallX, metalBallY, ballRadius);

    uint32_t woodBallX = (4 * world.data.width) / 5;
    uint32_t woodBallY = world.data.height / 10;
    addBall(world, woodBallX, woodBallY, ballRadius, MaterialType::WOOD);
    spdlog::info("Added wood ball at ({}, {}), radius {}", woodBallX, woodBallY, ballRadius);

    // Add random sand particles (5% of world space).
    uint32_t totalCells = world.data.width * world.data.height;
    uint32_t sandCellCount = static_cast<uint32_t>(totalCells * 0.05);
    uint32_t sandAdded = 0;

    // Use simple pseudo-random generation for reproducibility.
    std::srand(42); // Fixed seed for consistent benchmarks.

    while (sandAdded < sandCellCount) {
        uint32_t x = 1 + (std::rand() % (world.data.width - 2));
        uint32_t y = 1 + (std::rand() % (world.data.height - 2));

        // Only add sand to empty cells (don't overwrite water, balls, or walls).
        if (world.at(x, y).material_type == MaterialType::AIR && world.at(x, y).fill_ratio == 0.0) {
            world.at(x, y).replaceMaterial(MaterialType::SAND, 1.0);
            sandAdded++;
        }
    }
    spdlog::info("Added {} random sand particles (5% of {} cells)", sandAdded, totalCells);

    spdlog::info("BenchmarkScenario::setup complete");
}

void BenchmarkScenario::reset(World& world)
{
    spdlog::info("BenchmarkScenario::reset - resetting world");
    setup(world);
}

void BenchmarkScenario::addBall(
    World& world, uint32_t centerX, uint32_t centerY, uint32_t radius, MaterialType material)
{
    // Create a circular ball of material.
    for (uint32_t y = 0; y < world.data.height; ++y) {
        for (uint32_t x = 0; x < world.data.width; ++x) {
            // Calculate distance from center.
            int dx = static_cast<int>(x) - static_cast<int>(centerX);
            int dy = static_cast<int>(y) - static_cast<int>(centerY);
            double distance = std::sqrt(dx * dx + dy * dy);

            // If within radius, add material.
            if (distance <= static_cast<double>(radius)) {
                world.at(x, y).replaceMaterial(material, 1.0);
            }
        }
    }
}
