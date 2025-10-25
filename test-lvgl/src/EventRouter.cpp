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

void EventRouter::processImmediateEvent(const ToggleForceCommand& /*cmd. */)
{
    // Toggle force visualization state.
    auto params = sharedState_.getPhysicsParams();
    params.forceVisualizationEnabled = !params.forceVisualizationEnabled;
    sharedState_.updatePhysicsParams(params);

    spdlog::info(
        "Processing ToggleForceCommand - Force visualization now: {}",
        params.forceVisualizationEnabled);
}

void EventRouter::processImmediateEvent(const ToggleCohesionCommand& /*cmd. */)
{
    // Toggle cohesion physics state.
    auto params = sharedState_.getPhysicsParams();
    params.cohesionEnabled = !params.cohesionEnabled;
    sharedState_.updatePhysicsParams(params);

    spdlog::info(
        "Processing ToggleCohesionCommand - Cohesion physics now: {}", params.cohesionEnabled);
}

void EventRouter::processImmediateEvent(const ToggleCohesionForceCommand& /*cmd. */)
{
    // Toggle cohesion force physics state.
    auto params = sharedState_.getPhysicsParams();
    params.cohesionEnabled = !params.cohesionEnabled;
    sharedState_.updatePhysicsParams(params);

    spdlog::info(
        "Processing ToggleCohesionForceCommand - Cohesion force physics now: {}",
        params.cohesionEnabled);
}

void EventRouter::processImmediateEvent(const ToggleAdhesionCommand& /*cmd. */)
{
    // Toggle adhesion physics state and visualization.
    auto params = sharedState_.getPhysicsParams();
    params.adhesionEnabled = !params.adhesionEnabled;
    sharedState_.updatePhysicsParams(params);

    // Also toggle the adhesion vector visualization.
    Cell::adhesionDrawEnabled = params.adhesionEnabled;

    // Update world if available.
    auto* world = sharedState_.getCurrentWorld();
    if (world) {
        world->setAdhesionEnabled(params.adhesionEnabled);
    }

    spdlog::info(
        "Processing ToggleAdhesionCommand - Adhesion physics now: {}", params.adhesionEnabled);
}

void EventRouter::processImmediateEvent(const ToggleTimeHistoryCommand& /*cmd. */)
{
    // Toggle time history tracking.
    auto params = sharedState_.getPhysicsParams();
    params.timeHistoryEnabled = !params.timeHistoryEnabled;
    sharedState_.updatePhysicsParams(params);

    spdlog::info(
        "Processing ToggleTimeHistoryCommand - Time history now: {}", params.timeHistoryEnabled);
}
