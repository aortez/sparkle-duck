#include "core/Cell.h"
#include "core/World.h"
#include "core/WorldEventGenerator.h"
#include "server/scenarios/Scenario.h"
#include "server/scenarios/ScenarioRegistry.h"
#include "server/scenarios/ScenarioWorldEventGenerator.h"
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

    // DEPRECATED: Temporary compatibility.
    std::unique_ptr<WorldEventGenerator> createWorldEventGenerator() const override
    {
        // Create a ConfigurableWorldEventGenerator with config (generator owns it).
        auto configurableSetup = std::make_unique<ConfigurableWorldEventGenerator>(config_);

        // Apply non-config settings.
        configurableSetup->setWallsEnabled(true); // Walls always enabled for physics containment.
        configurableSetup->setMiddleMetalWallEnabled(false);
        configurableSetup->setLeftThrowEnabled(false);

        return configurableSetup;
    }

private:
    ScenarioMetadata metadata_;
    SandboxConfig config_;

    // Timing state for particle generation.
    double lastSimTime_ = 0.0;
    double nextRightThrow_ = 1.0;
    double nextRainDrop_ = 0.0;

    // Water column auto-disable state.
    double waterColumnStartTime_ = -1.0;
    static constexpr double WATER_COLUMN_DURATION = 2.0;

    // Helper methods (scenario-specific, naturally belong here!).
    void addWaterColumn(World& world);
    void clearWaterColumn(World& world);
    void addDirtQuadrant(World& world);
    void clearDirtQuadrant(World& world);
    void refillWaterColumn(World& world);
    void addRainDrops(World& world);
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
    nextRainDrop_ = 0.0;
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

    // Rain drops at variable rate (if rain rate > 0).
    if (config_.rain_rate > 0.0 && simTime >= nextRainDrop_) {
        addRainDrops(world);
        double intervalSeconds = 1.0 / config_.rain_rate;
        nextRainDrop_ = simTime + intervalSeconds;
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

void SandboxScenario::addRainDrops(World& world)
{
    spdlog::debug("Adding rain drop (rate: {:.1f}/s)", config_.rain_rate);

    // Use normal distribution for horizontal position.
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::normal_distribution<double> normalDist(0.5, 0.15);

    // Generate random position across the top, clamped to world bounds.
    double randomPos = std::max(0.0, std::min(1.0, normalDist(gen)));
    uint32_t xPos = static_cast<uint32_t>(randomPos * (world.data.width - 2)) + 1;

    if (xPos < world.data.width && 1 < world.data.height) {
        Cell& cell = world.at(xPos, 1); // Just below the top wall.
        cell.addWater(0.8);
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
