#include "State.h"
#include "../DirtSimStateMachine.h"
#include "../SimulationManager.h"
#include "../SimulatorUI.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace State {

void SimPaused::onEnter(DirtSimStateMachine& dsm) {
    spdlog::info("SimPaused: Entered pause state from SimRunning at step {}", previousState.stepCount);

    // Set the pause flag.
    dsm.getSharedState().setIsPaused(true);

    // Actually pause the simulation by setting timescale to 0.
    if (dsm.simulationManager && dsm.simulationManager->getWorld()) {
        // Store the current timescale before pausing.
        previousTimescale = dsm.simulationManager->getWorld()->getTimescale();
        dsm.simulationManager->getWorld()->setTimescale(0.0);
        spdlog::info("SimPaused: Set timescale to 0.0 (was {})", previousTimescale);
    }

    // Push UI update to change pause button label to "Resume".
    if (dsm.getSharedState().isPushUpdatesEnabled()) {
        UIUpdateEvent update = dsm.buildUIUpdate();
        update.dirty.uiState = true;
        update.isPaused = true;
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }
}

void SimPaused::onExit(DirtSimStateMachine& dsm) {
    spdlog::info("SimPaused: Exiting pause state");

    // Clear the pause flag.
    dsm.getSharedState().setIsPaused(false);

    // Restore the timescale to resume simulation.
    if (dsm.simulationManager && dsm.simulationManager->getWorld()) {
        dsm.simulationManager->getWorld()->setTimescale(previousTimescale);
        spdlog::info("SimPaused: Restored timescale to {}", previousTimescale);
    }

    // Push UI update to change pause button label back to "Pause".
    if (dsm.getSharedState().isPushUpdatesEnabled()) {
        UIUpdateEvent update = dsm.buildUIUpdate();
        update.dirty.uiState = true;
        update.isPaused = false;
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }
}

State::Any SimPaused::onEvent(const ResumeCommand& /*cmd*/, DirtSimStateMachine& /*dsm. */) {
    spdlog::info("SimPaused: Resuming to SimRunning at step {}", previousState.stepCount);
    
    return std::move(previousState);
}

State::Any SimPaused::onEvent(const ResetSimulationCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    spdlog::info("SimPaused: Resetting simulation");
    
    if (dsm.simulationManager) {
        dsm.simulationManager->reset();
    }
    
    // Return to running state with reset count.
    return SimRunning{};
}

State::Any SimPaused::onEvent(const AdvanceSimulationCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    // In paused state, we can still advance one step (for frame-by-frame debugging)
    if (dsm.simulationManager) {
        dsm.simulationManager->advanceTime(1.0/60.0);  // Single step.
        previousState.stepCount++;
        dsm.getSharedState().setCurrentStep(previousState.stepCount);
        
        spdlog::debug("SimPaused: Advanced one step to {}", previousState.stepCount);
        
        // Push UI update if push-based system is enabled.
        if (dsm.getSharedState().isPushUpdatesEnabled()) {
            dsm.getSharedState().pushUIUpdate(dsm.buildUIUpdate());
        }
    }
    
    return *this;  // Stay paused.
}

State::Any SimPaused::onEvent(const MouseDownEvent& evt, DirtSimStateMachine& dsm) {
    if (!dsm.simulationManager || !dsm.simulationManager->getWorld()) {
        return *this;
    }

    auto* world = dsm.simulationManager->getWorld();

    // Always enter GRAB_MODE - either grab existing material or create new and grab it.
    previousState.interactionMode = SimRunning::InteractionMode::GRAB_MODE;

    if (world->hasMaterialAtPixel(evt.pixelX, evt.pixelY)) {
        // Cell has material - grab it.
        world->startDragging(evt.pixelX, evt.pixelY);
        spdlog::debug("SimPaused MouseDown: Grabbing existing material at ({}, {})", evt.pixelX, evt.pixelY);
    } else {
        // Cell is empty - add material first, then grab it.
        auto material = dsm.getSharedState().getSelectedMaterial();
        world->addMaterialAtPixel(evt.pixelX, evt.pixelY, material);
        world->startDragging(evt.pixelX, evt.pixelY);
        spdlog::debug("SimPaused MouseDown: Creating and grabbing new {} at ({}, {})",
                      static_cast<int>(material), evt.pixelX, evt.pixelY);
    }

    return *this;
}

State::Any SimPaused::onEvent(const MouseMoveEvent& evt, DirtSimStateMachine& dsm) {
    if (!dsm.simulationManager || !dsm.simulationManager->getWorld()) {
        return *this;
    }

    auto* world = dsm.simulationManager->getWorld();

    // Only update drag position if we're in GRAB_MODE.
    if (previousState.interactionMode == SimRunning::InteractionMode::GRAB_MODE) {
        world->updateDrag(evt.pixelX, evt.pixelY);
    }

    return *this;
}

State::Any SimPaused::onEvent(const MouseUpEvent& evt, DirtSimStateMachine& dsm) {
    if (!dsm.simulationManager || !dsm.simulationManager->getWorld()) {
        return *this;
    }

    auto* world = dsm.simulationManager->getWorld();

    if (previousState.interactionMode == SimRunning::InteractionMode::GRAB_MODE) {
        // End dragging and release material with velocity.
        world->endDragging(evt.pixelX, evt.pixelY);
        spdlog::debug("SimPaused MouseUp: Ending GRAB_MODE at ({}, {})", evt.pixelX, evt.pixelY);
    }

    // Reset interaction mode.
    previousState.interactionMode = SimRunning::InteractionMode::NONE;

    return *this;
}

State::Any SimPaused::onEvent(const SelectMaterialCommand& cmd, DirtSimStateMachine& dsm) {
    dsm.getSharedState().setSelectedMaterial(cmd.material);
    return *this;
}

// Handle immediate events routed through push system.
State::Any SimPaused::onEvent(const GetFPSCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    // FPS is already tracked in shared state and will be in next push update.
    spdlog::debug("SimPaused: GetFPSCommand - FPS will be in next update");
    
    // Force a push update with FPS dirty flag.
    if (dsm.getSharedState().isPushUpdatesEnabled()) {
        UIUpdateEvent update = dsm.buildUIUpdate();
        update.dirty.fps = true;
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }
    
    return *this;
}

State::Any SimPaused::onEvent(const GetSimStatsCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    // Stats are already tracked and will be in next push update.
    spdlog::debug("SimPaused: GetSimStatsCommand - Stats will be in next update");
    
    // Force a push update with stats dirty flag.
    if (dsm.getSharedState().isPushUpdatesEnabled()) {
        UIUpdateEvent update = dsm.buildUIUpdate();
        update.dirty.stats = true;
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }
    
    return *this;
}

State::Any SimPaused::onEvent(const ToggleDebugCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    // Toggle debug draw state.
    auto params = dsm.getSharedState().getPhysicsParams();
    params.debugEnabled = !params.debugEnabled;
    dsm.getSharedState().updatePhysicsParams(params);
    spdlog::debug("SimPaused: ToggleDebugCommand - Debug draw now: {}", params.debugEnabled);
    
    // Force a push update with uiState dirty flag.
    if (dsm.getSharedState().isPushUpdatesEnabled()) {
        UIUpdateEvent update = dsm.buildUIUpdate();
        update.dirty.uiState = true;
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }
    
    return *this;
}

State::Any SimPaused::onEvent(const ToggleForceCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    auto params = dsm.getSharedState().getPhysicsParams();
    params.forceVisualizationEnabled = !params.forceVisualizationEnabled;
    dsm.getSharedState().updatePhysicsParams(params);
    spdlog::debug("SimPaused: ToggleForceCommand - Force viz now: {}", params.forceVisualizationEnabled);
    
    // Force a push update with physics params dirty flag.
    if (dsm.getSharedState().isPushUpdatesEnabled()) {
        UIUpdateEvent update = dsm.buildUIUpdate();
        update.dirty.physicsParams = true;
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }
    
    return *this;
}

State::Any SimPaused::onEvent(const ToggleCohesionCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    auto params = dsm.getSharedState().getPhysicsParams();
    params.cohesionEnabled = !params.cohesionEnabled;
    dsm.getSharedState().updatePhysicsParams(params);
    spdlog::debug("SimPaused: ToggleCohesionCommand - Cohesion now: {}", params.cohesionEnabled);

    // Force a push update with physics params dirty flag.
    if (dsm.getSharedState().isPushUpdatesEnabled()) {
        UIUpdateEvent update = dsm.buildUIUpdate();
        update.dirty.physicsParams = true;
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }

    return *this;
}

State::Any SimPaused::onEvent(const ToggleCohesionForceCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    auto params = dsm.getSharedState().getPhysicsParams();
    params.cohesionEnabled = !params.cohesionEnabled;
    dsm.getSharedState().updatePhysicsParams(params);
    spdlog::debug("SimPaused: ToggleCohesionForceCommand - Cohesion force now: {}", params.cohesionEnabled);

    // Force a push update with physics params dirty flag.
    if (dsm.getSharedState().isPushUpdatesEnabled()) {
        UIUpdateEvent update = dsm.buildUIUpdate();
        update.dirty.physicsParams = true;
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }

    return *this;
}

State::Any SimPaused::onEvent(const ToggleAdhesionCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    auto params = dsm.getSharedState().getPhysicsParams();
    params.adhesionEnabled = !params.adhesionEnabled;
    dsm.getSharedState().updatePhysicsParams(params);
    
    // Update world if available.
    if (dsm.simulationManager && dsm.simulationManager->getWorld()) {
        dsm.simulationManager->getWorld()->setAdhesionEnabled(params.adhesionEnabled);
    }
    
    spdlog::debug("SimPaused: ToggleAdhesionCommand - Adhesion now: {}", params.adhesionEnabled);
    
    // Force a push update with physics params dirty flag.
    if (dsm.getSharedState().isPushUpdatesEnabled()) {
        UIUpdateEvent update = dsm.buildUIUpdate();
        update.dirty.physicsParams = true;
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }
    
    return *this;
}

State::Any SimPaused::onEvent(const ToggleTimeHistoryCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    auto params = dsm.getSharedState().getPhysicsParams();
    params.timeHistoryEnabled = !params.timeHistoryEnabled;
    dsm.getSharedState().updatePhysicsParams(params);
    spdlog::debug("SimPaused: ToggleTimeHistoryCommand - Time history now: {}", params.timeHistoryEnabled);
    
    // Force a push update with physics params dirty flag.
    if (dsm.getSharedState().isPushUpdatesEnabled()) {
        UIUpdateEvent update = dsm.buildUIUpdate();
        update.dirty.physicsParams = true;
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }
    
    return *this;
}

State::Any SimPaused::onEvent(const PrintAsciiDiagramCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    // Get the current world and print ASCII diagram.
    if (dsm.simulationManager && dsm.simulationManager->getWorld()) {
        std::string ascii_diagram = dsm.simulationManager->getWorld()->toAsciiDiagram();
        spdlog::info("Current world state (ASCII diagram):\n{}", ascii_diagram);
    }
    else {
        spdlog::warn("PrintAsciiDiagramCommand: No world available");
    }
    
    return *this;
}

State::Any SimPaused::onEvent(const QuitApplicationCommand& /*cmd*/, DirtSimStateMachine& /*dsm*/) {
    spdlog::info("SimPaused: Quit application requested");

    // Take exit screenshot before quitting.
    SimulatorUI::takeExitScreenshot();

    // Transition to Shutdown state.
    return Shutdown{};
}

} // namespace State.
} // namespace DirtSim.