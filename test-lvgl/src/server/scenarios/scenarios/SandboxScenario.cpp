#include "core/Cell.h"
#include "core/World.h"
#include "server/scenarios/Scenario.h"
#include "server/scenarios/ScenarioRegistry.h"
#include "spdlog/spdlog.h"
#include <random>

using namespace DirtSim;

/**
 * Sandbox scenario - The default world setup without walls.
 */
class SandboxScenario : public Scenario {
public:
    SandboxScenario()
    {
        metadata_.name = "Sandbox";
        metadata_.description =
            "Default sandbox with dirt quadrant and particle streams (no walls)";
        metadata_.category = "sandbox";

        // Initialize with default config.
        config_.quadrant_enabled = true;
        config_.water_column_enabled = true;
        config_.right_throw_enabled = true;
        config_.rain_rate = 0.0;
    }

    const ScenarioMetadata& getMetadata() const override { return metadata_; }

    ScenarioConfig getConfig() const override { return config_; }

    void setConfig(const ScenarioConfig& newConfig, World& world) override
    {
        // Validate type and update.
        if (std::holds_alternative<SandboxConfig>(newConfig)) {
            const SandboxConfig& newSandboxConfig = std::get<SandboxConfig>(newConfig);

            // Check if water column state changed.
            bool wasWaterEnabled = config_.water_column_enabled;
            bool nowWaterEnabled = newSandboxConfig.water_column_enabled;

            // Check if quadrant state changed.
            bool wasQuadrantEnabled = config_.quadrant_enabled;
            bool nowQuadrantEnabled = newSandboxConfig.quadrant_enabled;

            // Update config.
            config_ = newSandboxConfig;

            // Apply water column changes immediately.
            if (!wasWaterEnabled && nowWaterEnabled) {
                waterColumnStartTime_ = 0.0;
                addWaterColumn(world);
                spdlog::info("SandboxScenario: Water column enabled and added");
            }
            else if (wasWaterEnabled && !nowWaterEnabled) {
                waterColumnStartTime_ = -1.0;
                clearWaterColumn(world);
                spdlog::info("SandboxScenario: Water column disabled and cleared");
            }

            // Apply quadrant changes immediately.
            if (!wasQuadrantEnabled && nowQuadrantEnabled) {
                addDirtQuadrant(world);
                spdlog::info("SandboxScenario: Dirt quadrant enabled and added");
            }
            else if (wasQuadrantEnabled && !nowQuadrantEnabled) {
                clearDirtQuadrant(world);
                spdlog::info("SandboxScenario: Dirt quadrant disabled and cleared");
            }

            spdlog::info("SandboxScenario: Config updated");
        }
        else {
            spdlog::error("SandboxScenario: Invalid config type provided");
        }
    }

    void setup(World& world) override;
    void reset(World& world) override;
    void tick(World& world, double deltaTime) override;

private:
    ScenarioMetadata metadata_;
    SandboxConfig config_;

    // Timing state for particle generation.
    double lastSimTime_ = 0.0;
    double nextRightThrow_ = 1.0;

    // Water column auto-disable state.
    double waterColumnStartTime_ = -1.0;
    static constexpr double WATER_COLUMN_DURATION = 2.0;

    // Random number generator for rain drops.
    std::mt19937 rng_{ std::random_device{}() };

    // Helper methods (scenario-specific, naturally belong here!).
    void addWaterColumn(World& world);
    void clearWaterColumn(World& world);
    void addDirtQuadrant(World& world);
    void clearDirtQuadrant(World& world);
    void refillWaterColumn(World& world);
    void addRainDrops(World& world, double deltaTime);
    void spawnWaterDrop(
        World& world, uint32_t centerX, uint32_t centerY, double radius, double fillAmount);
    void throwDirtBalls(World& world);
};

// ============================================================================
// SandboxScenario Implementation
// ============================================================================

void SandboxScenario::setup(World& world)
{
    spdlog::info("SandboxScenario::setup - initializing world");

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

    // Fill lower-right quadrant if enabled.
    if (config_.quadrant_enabled) {
        addDirtQuadrant(world);
    }

    // Add water column if enabled.
    if (config_.water_column_enabled) {
        addWaterColumn(world);
        // Initialize water column start time.
        waterColumnStartTime_ = 0.0;
    }

    spdlog::info("SandboxScenario::setup complete");
}

void SandboxScenario::reset(World& world)
{
    spdlog::info("SandboxScenario::reset - resetting world and timers");

    // Reset timing state.
    lastSimTime_ = 0.0;
    nextRightThrow_ = 1.0;
    waterColumnStartTime_ = -1.0;

    // Re-run setup to reinitialize world.
    setup(world);
}

void SandboxScenario::tick(World& world, double deltaTime)
{
    const double simTime = lastSimTime_ + deltaTime;

    // Recurring throws from right side every ~0.83 seconds (if enabled).
    const double throwPeriod = 0.83;
    if (config_.right_throw_enabled && simTime >= nextRightThrow_) {
        throwDirtBalls(world);
        nextRightThrow_ += throwPeriod;
    }

    // Rain drops - time-scale independent, probability-based.
    if (config_.rain_rate > 0.0) {
        addRainDrops(world, deltaTime);
    }

    // Water column refill (if enabled).
    if (config_.water_column_enabled) {
        // Initialize start time on first call.
        if (waterColumnStartTime_ == 0.0) {
            waterColumnStartTime_ = simTime;
            spdlog::info(
                "Water column starting at time {:.3f}s (will auto-disable after {:.1f}s)",
                waterColumnStartTime_,
                WATER_COLUMN_DURATION);
        }

        // Check for auto-disable timeout.
        if (waterColumnStartTime_ > 0.0) {
            double elapsed = simTime - waterColumnStartTime_;
            if (elapsed >= WATER_COLUMN_DURATION) {
                spdlog::info(
                    "Water column auto-disabling after {:.1f} seconds (elapsed: {:.1f}s)",
                    WATER_COLUMN_DURATION,
                    elapsed);
                config_.water_column_enabled = false;
                waterColumnStartTime_ = -1.0;
            }
        }

        // Refill if still enabled.
        if (config_.water_column_enabled) {
            refillWaterColumn(world);
        }
    }

    lastSimTime_ = simTime;
}

void SandboxScenario::addWaterColumn(World& world)
{
    // Scale water column dimensions based on world size.
    uint32_t columnWidth = std::max(3u, std::min(8u, world.data.width / 20));
    uint32_t columnHeight = world.data.height / 3;

    // Add water column on left side.
    for (uint32_t y = 0; y < columnHeight && y < world.data.height; ++y) {
        for (uint32_t x = 1; x <= columnWidth && x < world.data.width; ++x) {
            world.at(x, y).addWater(1.0);
        }
    }
    spdlog::info("Added water column ({} wide × {} tall) on left side", columnWidth, columnHeight);
}

void SandboxScenario::clearWaterColumn(World& world)
{
    // Scale water column dimensions based on world size.
    uint32_t columnWidth = std::max(3u, std::min(8u, world.data.width / 20));
    uint32_t columnHeight = world.data.height / 3;

    // Clear water from the water column area.
    for (uint32_t y = 0; y < columnHeight && y < world.data.height; ++y) {
        for (uint32_t x = 1; x <= columnWidth && x < world.data.width; ++x) {
            Cell& cell = world.at(x, y);
            if (cell.material_type == MaterialType::WATER) {
                cell.replaceMaterial(MaterialType::AIR, 0.0);
            }
        }
    }
    spdlog::info("Cleared water column");
}

void SandboxScenario::addDirtQuadrant(World& world)
{
    // Fill lower-right quadrant with dirt.
    uint32_t startX = world.data.width / 2;
    uint32_t startY = world.data.height / 2;
    for (uint32_t y = startY; y < world.data.height - 1; ++y) {
        for (uint32_t x = startX; x < world.data.width - 1; ++x) {
            world.at(x, y).addDirt(1.0);
        }
    }
    spdlog::info("Added dirt quadrant ({}x{} cells)", world.data.width / 2, world.data.height / 2);
}

void SandboxScenario::clearDirtQuadrant(World& world)
{
    // Clear dirt from lower-right quadrant.
    uint32_t startX = world.data.width / 2;
    uint32_t startY = world.data.height / 2;
    for (uint32_t y = startY; y < world.data.height - 1; ++y) {
        for (uint32_t x = startX; x < world.data.width - 1; ++x) {
            Cell& cell = world.at(x, y);
            if (cell.material_type == MaterialType::DIRT) {
                cell.replaceMaterial(MaterialType::AIR, 0.0);
            }
        }
    }
    spdlog::info("Cleared dirt quadrant");
}

void SandboxScenario::refillWaterColumn(World& world)
{
    // Scale water column dimensions based on world size.
    uint32_t columnWidth = std::max(3u, std::min(8u, world.data.width / 20));
    uint32_t columnHeight = world.data.height / 3;

    // Refill any empty or water cells in the water column area.
    for (uint32_t y = 0; y < columnHeight && y < world.data.height; ++y) {
        for (uint32_t x = 1; x <= columnWidth && x < world.data.width; ++x) {
            Cell& cell = world.at(x, y);
            // Only refill if cell is air or water, and not already full.
            if ((cell.material_type == MaterialType::AIR
                 || cell.material_type == MaterialType::WATER)
                && !cell.isFull()) {
                cell.addWater(1.0 - cell.fill_ratio);
            }
        }
    }
}

void SandboxScenario::addRainDrops(World& world, double deltaTime)
{
    // Normalize rain_rate from [0, 10] to [0, 1].
    double normalized_rate = config_.rain_rate / 10.0;
    if (normalized_rate <= 0.0) {
        return;
    }

    // Scale with world width so larger worlds get proportionally more rain.
    double widthScale = world.data.width / 20.0;

    // Drop count scales linearly with rain_rate (more rain = more drops).
    const double baseDropsPerSecond = 3.0; // Tunable drop frequency.
    double expectedDrops = config_.rain_rate * baseDropsPerSecond * deltaTime * widthScale;

    // Drop radius scales quadratically with rain_rate AND proportionally with world width.
    // Low rate → tiny misting drops, high rate → large concentrated drops.
    // Larger worlds → proportionally larger drops (keeps visual consistency).
    const double scalar_factor = 5.0;
    double baseRadius = normalized_rate * normalized_rate * scalar_factor;
    double meanRadius = baseRadius * widthScale;

    // Total water rate scales quadratically (matches drop size scaling).
    // rain_rate=1 → 0.01× water, rain_rate=5 → 0.25× water, rain_rate=10 → 1.0× water.
    const double baseWaterConstant = 50.0; // Tunable wetness factor.
    double targetWaterRate = normalized_rate * normalized_rate * baseWaterConstant * widthScale;

    // Variance: normal distribution with rate-dependent std deviation.
    // More variance at high rates (20% to 50% std).
    double stdRadius = meanRadius * (0.2 + normalized_rate * 0.3);
    std::normal_distribution<double> radiusDist(meanRadius, stdRadius);

    // Use Poisson distribution for realistic randomness in drop count.
    std::poisson_distribution<int> poissonDrops(std::max(0.0, expectedDrops));
    int numDrops = poissonDrops(rng_);

    if (numDrops == 0) {
        return;
    }

    // Calculate fill amount to achieve target water rate.
    // fill = targetWater / (drops × area)
    double meanDropArea = M_PI * meanRadius * meanRadius;
    double fillAmount = (targetWaterRate * deltaTime) / (numDrops * meanDropArea);
    fillAmount = std::clamp(fillAmount, 0.01, 1.0);

    spdlog::debug(
        "Adding {} rain drops (rate: {:.1f}, meanRadius: {:.2f}, fill: {:.2f}, deltaTime: {:.3f}s)",
        numDrops,
        config_.rain_rate,
        meanRadius,
        fillAmount,
        deltaTime);

    // Uniform distributions for position.
    std::uniform_int_distribution<uint32_t> xDist(1, world.data.width - 2);

    // Top 15% of world for vertical spawning (minimum 3 cells).
    uint32_t maxY = std::max(3u, static_cast<uint32_t>(world.data.height * 0.15));
    std::uniform_int_distribution<uint32_t> yDist(1, maxY);

    // Spawn drops with varying sizes.
    for (int i = 0; i < numDrops; i++) {
        uint32_t x = xDist(rng_);
        uint32_t y = yDist(rng_);

        // Sample radius from normal distribution (each drop varies).
        double dropRadius = radiusDist(rng_);
        dropRadius = std::max(0.01, dropRadius); // Very small minimum for misting.

        // Use the calculated fill amount for all drops.
        spawnWaterDrop(world, x, y, dropRadius, fillAmount);
    }
}

void SandboxScenario::spawnWaterDrop(
    World& world, uint32_t centerX, uint32_t centerY, double radius, double fillAmount)
{
    // Spawn a circular water drop with specified fill amount.
    // Tiny drops (radius < 1) create misting effect with partial fills.
    // Large drops (radius ≥ 1) fill cells completely.

    // Calculate bounding box (only scan area that could be affected).
    uint32_t radiusInt = static_cast<uint32_t>(std::ceil(radius));
    uint32_t minX = centerX > radiusInt ? centerX - radiusInt : 0;
    uint32_t maxX = std::min(centerX + radiusInt, world.data.width - 1);
    uint32_t minY = centerY > radiusInt ? centerY - radiusInt : 0;
    uint32_t maxY = std::min(centerY + radiusInt, world.data.height - 1);

    for (uint32_t y = minY; y <= maxY; ++y) {
        for (uint32_t x = minX; x <= maxX; ++x) {
            // Calculate distance from center.
            int dx = static_cast<int>(x) - static_cast<int>(centerX);
            int dy = static_cast<int>(y) - static_cast<int>(centerY);
            double distance = std::sqrt(dx * dx + dy * dy);

            // If within radius, add water with specified fill amount.
            if (distance <= radius) {
                world.at(x, y).addWater(fillAmount);
            }
        }
    }
}

void SandboxScenario::throwDirtBalls(World& world)
{
    spdlog::debug("Adding right periodic throw");
    uint32_t rightX = world.data.width - 3;
    int32_t centerYSigned = static_cast<int32_t>(world.data.height) / 2 - 2;
    if (rightX < world.data.width && centerYSigned >= 0) {
        uint32_t centerY = static_cast<uint32_t>(centerYSigned);
        Cell& cell = world.at(rightX, centerY);
        cell.addDirtWithVelocity(1.0, Vector2d{ -10, -10 });
    }
}
