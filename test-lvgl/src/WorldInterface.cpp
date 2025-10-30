#include "WorldInterface.h"
#include "MaterialType.h"
#include "WorldDiagramGeneratorEmoji.h"
#include "WorldSetup.h"
#include "spdlog/spdlog.h"

void WorldInterface::initializeWorldSetup()
{
    worldSetup_ = std::make_unique<ConfigurableWorldSetup>();
}

void WorldInterface::setup()
{
    spdlog::info("Setting up {} with initial materials", "World");

    // First reset to empty state.
    reset();

    // Use the world setup strategy to initialize the world.
    if (worldSetup_) {
        worldSetup_->setup(*this);
    }
    else {
        spdlog::warn("WorldSetup is null in {}::setup()", "World");
    }
}

void WorldInterface::spawnMaterialBall(MaterialType type, uint32_t centerX, uint32_t centerY, uint32_t radius)
{
    // Spawn a (2*radius+1) x (2*radius+1) square of material centered at (centerX, centerY).
    int r = static_cast<int>(radius);
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            uint32_t x = centerX + dx;
            uint32_t y = centerY + dy;

            // Check bounds.
            if (x < getWidth() && y < getHeight()) {
                addMaterialAtCell(x, y, type, 1.0);
            }
        }
    }

    spdlog::info("Spawned {}x{} {} ball at center ({}, {})",
                 2*radius+1, 2*radius+1, getMaterialName(type), centerX, centerY);
}

// =================================================================
// WORLD SETUP CONTROLS - Default implementations.
// =================================================================

void WorldInterface::setLeftThrowEnabled(bool enabled)
{
    ConfigurableWorldSetup* configSetup = dynamic_cast<ConfigurableWorldSetup*>(worldSetup_.get());
    if (configSetup) {
        configSetup->setLeftThrowEnabled(enabled);
    }
}

void WorldInterface::setRightThrowEnabled(bool enabled)
{
    ConfigurableWorldSetup* configSetup = dynamic_cast<ConfigurableWorldSetup*>(worldSetup_.get());
    if (configSetup) {
        configSetup->setRightThrowEnabled(enabled);
    }
}

void WorldInterface::setLowerRightQuadrantEnabled(bool enabled)
{
    ConfigurableWorldSetup* configSetup = dynamic_cast<ConfigurableWorldSetup*>(worldSetup_.get());
    if (configSetup) {
        configSetup->setLowerRightQuadrantEnabled(enabled);
    }
}

void WorldInterface::setWallsEnabled(bool enabled)
{
    ConfigurableWorldSetup* configSetup = dynamic_cast<ConfigurableWorldSetup*>(worldSetup_.get());
    if (configSetup) {
        configSetup->setWallsEnabled(enabled);
    }
}

void WorldInterface::setRainRate(double rate)
{
    ConfigurableWorldSetup* configSetup = dynamic_cast<ConfigurableWorldSetup*>(worldSetup_.get());
    if (configSetup) {
        configSetup->setRainRate(rate);
    }
}

void WorldInterface::setWaterColumnEnabled(bool enabled)
{
    ConfigurableWorldSetup* configSetup = dynamic_cast<ConfigurableWorldSetup*>(worldSetup_.get());
    if (configSetup) {
        configSetup->setWaterColumnEnabled(enabled);
        spdlog::info("WorldInterface: Set water column enabled = {} (ConfigurableWorldSetup found)", enabled);
    } else {
        spdlog::warn("WorldInterface: Cannot set water column - ConfigurableWorldSetup not available");
    }
}

bool WorldInterface::isLeftThrowEnabled() const
{
    const ConfigurableWorldSetup* configSetup =
        dynamic_cast<const ConfigurableWorldSetup*>(worldSetup_.get());
    return configSetup ? configSetup->isLeftThrowEnabled() : false;
}

bool WorldInterface::isRightThrowEnabled() const
{
    const ConfigurableWorldSetup* configSetup =
        dynamic_cast<const ConfigurableWorldSetup*>(worldSetup_.get());
    return configSetup ? configSetup->isRightThrowEnabled() : false;
}

bool WorldInterface::isLowerRightQuadrantEnabled() const
{
    const ConfigurableWorldSetup* configSetup =
        dynamic_cast<const ConfigurableWorldSetup*>(worldSetup_.get());
    return configSetup ? configSetup->isLowerRightQuadrantEnabled() : false;
}

bool WorldInterface::areWallsEnabled() const
{
    const ConfigurableWorldSetup* configSetup =
        dynamic_cast<const ConfigurableWorldSetup*>(worldSetup_.get());
    return configSetup ? configSetup->areWallsEnabled() : false;
}

double WorldInterface::getRainRate() const
{
    const ConfigurableWorldSetup* configSetup =
        dynamic_cast<const ConfigurableWorldSetup*>(worldSetup_.get());
    return configSetup ? configSetup->getRainRate() : 0.0;
}

bool WorldInterface::isWaterColumnEnabled() const
{
    const ConfigurableWorldSetup* configSetup =
        dynamic_cast<const ConfigurableWorldSetup*>(worldSetup_.get());
    return configSetup ? configSetup->isWaterColumnEnabled() : false;
}

std::string WorldInterface::toAsciiDiagram() const
{
    return WorldDiagramGeneratorEmoji::generateMixedDiagram(*this);
}

bool WorldInterface::shouldResize(uint32_t newWidth, uint32_t newHeight) const
{
    if (newWidth == getWidth() && newHeight == getHeight()) {
        spdlog::debug("Resize requested but dimensions unchanged: {}x{}", getWidth(), getHeight());
        return false;
    }

    spdlog::info(
        "Resizing {} grid: {}x{} -> {}x{}",
        "World",
        getWidth(),
        getHeight(),
        newWidth,
        newHeight);
    return true;
}