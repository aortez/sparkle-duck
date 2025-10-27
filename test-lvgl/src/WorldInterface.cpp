#include "WorldInterface.h"
#include "WorldDiagramGeneratorEmoji.h"
#include "WorldSetup.h"
#include "spdlog/spdlog.h"

void WorldInterface::initializeWorldSetup()
{
    worldSetup_ = std::make_unique<ConfigurableWorldSetup>();
}

void WorldInterface::setup()
{
    spdlog::info("Setting up {} with initial materials", getWorldTypeName());

    // First reset to empty state.
    reset();

    // Use the world setup strategy to initialize the world.
    if (worldSetup_) {
        worldSetup_->setup(*this);
    }
    else {
        spdlog::warn("WorldSetup is null in {}::setup()", getWorldTypeName());
    }
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
        getWorldTypeName(),
        getWidth(),
        getHeight(),
        newWidth,
        newHeight);
    return true;
}