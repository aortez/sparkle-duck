#include "EventRouter.h"
#include "Cell.h"
#include "WorldInterface.h"
#include <spdlog/spdlog.h>

void EventRouter::processImmediateEvent(const GetFPSCommand& /*cmd. */)
{
    // Get current FPS from shared state.
    float fps = sharedState_.getCurrentFPS();

    spdlog::info("Processing GetFPSCommand - Current FPS: {:.1f}", fps);

    // TODO: If this command has a callback, send the response.
    // For now, just log the value.
}

void EventRouter::processImmediateEvent(const GetSimStatsCommand& /*cmd. */)
{
    // Get simulation statistics from shared state.
    auto stats = sharedState_.getStats();

    spdlog::info(
        "Processing GetSimStatsCommand - Total cells: {}, Active cells: {}, Step: {}",
        stats.totalCells,
        stats.activeCells,
        stats.stepCount);

    // TODO: If this command has a callback, send the response.
    // For now, just log the stats.
}

void EventRouter::processImmediateEvent(const PauseCommand& /*cmd. */)
{
    // Set pause state.
    bool wasPaused = sharedState_.getIsPaused();
    sharedState_.setIsPaused(true);

    spdlog::info("Processing PauseCommand - Was paused: {}, Now paused: true", wasPaused);

    // Note: The actual pausing of the simulation loop happens.
    // when the simulation thread checks the pause state.
}

void EventRouter::processImmediateEvent(const ResumeCommand& /*cmd. */)
{
    // Clear pause state.
    bool wasPaused = sharedState_.getIsPaused();
    sharedState_.setIsPaused(false);

    spdlog::info("Processing ResumeCommand - Was paused: {}, Now paused: false", wasPaused);

    // Note: The actual resuming of the simulation loop happens.
    // when the simulation thread checks the pause state.
}

void EventRouter::processImmediateEvent(const PrintAsciiDiagramCommand& /*cmd. */)
{
    // Get the current world from shared state.
    auto* world = sharedState_.getCurrentWorld();
    if (world) {
        std::string ascii_diagram = world->toAsciiDiagram();
        spdlog::info("Current world state (ASCII diagram):\n{}", ascii_diagram);
    }
    else {
        spdlog::warn("PrintAsciiDiagramCommand: No world available");
    }
}

void EventRouter::processImmediateEvent(const ToggleCohesionCommand& /*cmd. */)
{
    auto* world = sharedState_.getCurrentWorld();
    if (world) {
        bool newValue = !world->isCohesionComForceEnabled();
        world->setCohesionComForceEnabled(newValue);
        spdlog::info("Processing ToggleCohesionCommand - Cohesion physics now: {}", newValue);
    }
}

void EventRouter::processImmediateEvent(const ToggleCohesionForceCommand& /*cmd. */)
{
    auto* world = sharedState_.getCurrentWorld();
    if (world) {
        bool newValue = !world->isCohesionComForceEnabled();
        world->setCohesionComForceEnabled(newValue);
        spdlog::info(
            "Processing ToggleCohesionForceCommand - Cohesion force physics now: {}", newValue);
    }
}

void EventRouter::processImmediateEvent(const ToggleAdhesionCommand& /*cmd. */)
{
    auto* world = sharedState_.getCurrentWorld();
    if (world) {
        bool newValue = !world->isAdhesionEnabled();
        world->setAdhesionEnabled(newValue);
        Cell::adhesionDrawEnabled = newValue;
        spdlog::info("Processing ToggleAdhesionCommand - Adhesion physics now: {}", newValue);
    }
}

void EventRouter::processImmediateEvent(const ToggleTimeHistoryCommand& /*cmd. */)
{
    auto* world = sharedState_.getCurrentWorld();
    if (world) {
        bool newValue = !world->isTimeReversalEnabled();
        world->enableTimeReversal(newValue);
        spdlog::info("Processing ToggleTimeHistoryCommand - Time history now: {}", newValue);
    }
}

void EventRouter::processImmediateEvent(const SetCellSizeCommand& cmd)
{
    auto* world = sharedState_.getCurrentWorld();
    if (world) {
        spdlog::info("Processing SetCellSizeCommand - Setting cell size to {}", cmd.size);
        Cell::setSize(static_cast<uint32_t>(cmd.size));

        // Recalculate grid dimensions based on new cell size.
        const int DRAW_AREA_SIZE = 850;
        const int new_grid_width = (DRAW_AREA_SIZE / static_cast<int>(cmd.size)) - 1;
        const int new_grid_height = (DRAW_AREA_SIZE / static_cast<int>(cmd.size)) - 1;

        // Resize the world grid (safe on UI thread).
        world->resizeGrid(new_grid_width, new_grid_height);
        world->markAllCellsDirty();

        spdlog::info(
            "Processing SetCellSizeCommand - Resized grid to {}x{} cells",
            new_grid_width,
            new_grid_height);
    }
}
