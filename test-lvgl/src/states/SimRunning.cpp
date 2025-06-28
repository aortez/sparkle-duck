#include "State.h"
#include "../DirtSimStateMachine.h"
#include "../SimulationManager.h"
#include "../SimulatorUI.h"
#include "../WorldFactory.h"
#include "../WorldSetup.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace State {

void SimRunning::onEnter(DirtSimStateMachine& dsm) {
    spdlog::info("SimRunning: Creating SimulationManager");
    
    // Determine screen/container for UI.
    lv_obj_t* screen = nullptr;
    if (lv_is_initialized() && dsm.display) {
        screen = lv_scr_act();
    }
    
    // Create SimulationManager with default world type and size.
    // TODO: These should come from configuration or previous state.
    // Grid size calculation matches main.cpp (based on 850px draw area)
    const int grid_width = 7;   // (850 / 100) - 1, where 100 is Cell::WIDTH.
    const int grid_height = 7;  // (850 / 100) - 1, where 100 is Cell::HEIGHT.
    WorldType worldType = WorldType::RulesB;  // Default.
    
    dsm.simulationManager = std::make_unique<SimulationManager>(
        worldType, grid_width, grid_height, screen);
    
    // Initialize the simulation.
    dsm.simulationManager->initialize();
    
    // Initialize step count from shared state (preserves count when resuming from pause)
    stepCount = dsm.getSharedState().getCurrentStep();
    
    // Only reset if we're starting fresh (not resuming from pause)
    if (stepCount == 0) {
        spdlog::debug("SimRunning: Starting fresh with step count 0");
    } else {
        spdlog::info("SimRunning: Resuming at step {}", stepCount);
    }
    
    spdlog::info("SimRunning: SimulationManager created, simulation ready");
}

void SimRunning::onExit(DirtSimStateMachine& /*dsm. */) {
    spdlog::info("SimRunning: Exiting state");
    
    // Note: We don't destroy SimulationManager here anymore.
    // It will be destroyed by states that actually need to destroy it.
    // (like when transitioning to MainMenu or Shutdown).
    // SimPaused needs the SimulationManager to remain alive.
}


State::Any SimRunning::onEvent(const AdvanceSimulationCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    if (!dsm.simulationManager) {
        spdlog::error("SimRunning: Cannot advance - no SimulationManager!");
        return *this;
    }
    
    // Advance the simulation.
    dsm.simulationManager->advanceTime(1.0/60.0);  // 60 FPS timestep.
    stepCount++;
    
    // Update shared state step count.
    dsm.getSharedState().setCurrentStep(stepCount);
    
    // Update shared state statistics periodically.
    if (stepCount % 60 == 0) {
        SimulationStats stats;
        stats.stepCount = stepCount;
        auto* world = dsm.simulationManager->getWorld();
        stats.totalCells = world->getWidth() * world->getHeight();
        // TODO: Get more detailed stats from world.
        
        dsm.getSharedState().updateStats(stats);
    }
    
    // Update FPS.
    // TODO: Calculate actual FPS.
    dsm.getSharedState().setCurrentFPS(60.0f);
    
    // Push UI update if push-based system is enabled.
    if (dsm.getSharedState().isPushUpdatesEnabled()) {
        dsm.getSharedState().pushUIUpdate(dsm.buildUIUpdate());
    }
    
    return *this;  // Stay in SimRunning.
}

State::Any SimRunning::onEvent(const PauseCommand& /*cmd*/, DirtSimStateMachine& /*dsm. */) {
    spdlog::info("SimRunning: Pausing at step {}", stepCount);
    
    // Move the current state into SimPaused.
    return SimPaused{std::move(*this)};
}

State::Any SimRunning::onEvent(const ResetSimulationCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    spdlog::info("SimRunning: Resetting simulation");
    
    if (dsm.simulationManager) {
        dsm.simulationManager->reset();
    }
    
    stepCount = 0;
    dsm.getSharedState().setCurrentStep(0);
    
    return *this;  // Stay in SimRunning.
}

State::Any SimRunning::onEvent(const SaveWorldCommand& cmd, DirtSimStateMachine& /*dsm. */) {
    Saving saveState;
    saveState.filepath = cmd.filepath;
    return saveState;
}

State::Any SimRunning::onEvent(const MouseDownEvent& evt, DirtSimStateMachine& dsm) {
    if (dsm.simulationManager && dsm.simulationManager->getWorld()) {
        // For now, just use the pixel coordinates directly.
        // The world will handle conversion internally.
        auto material = dsm.getSharedState().getSelectedMaterial();
        dsm.simulationManager->getWorld()->addMaterialAtPixel(evt.pixelX, evt.pixelY, material);
    }
    
    return *this;
}

State::Any SimRunning::onEvent(const SelectMaterialCommand& cmd, DirtSimStateMachine& dsm) {
    dsm.getSharedState().setSelectedMaterial(cmd.material);
    if (dsm.simulationManager && dsm.simulationManager->getWorld()) {
        dsm.simulationManager->getWorld()->setSelectedMaterial(cmd.material);
    }
    spdlog::debug("SimRunning: Selected material {}", static_cast<int>(cmd.material));
    return *this;
}

State::Any SimRunning::onEvent(const SetTimescaleCommand& cmd, DirtSimStateMachine& dsm) {
    auto params = dsm.getSharedState().getPhysicsParams();
    params.timescale = cmd.timescale;
    dsm.getSharedState().updatePhysicsParams(params);
    spdlog::debug("SimRunning: Set timescale to {}", cmd.timescale);
    return *this;
}

State::Any SimRunning::onEvent(const SetElasticityCommand& cmd, DirtSimStateMachine& dsm) {
    auto params = dsm.getSharedState().getPhysicsParams();
    params.elasticity = cmd.elasticity;
    dsm.getSharedState().updatePhysicsParams(params);
    
    // Apply to world.
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            world->setElasticityFactor(cmd.elasticity);
        }
    }
    
    spdlog::debug("SimRunning: Set elasticity to {}", cmd.elasticity);
    return *this;
}

State::Any SimRunning::onEvent(const SetDynamicStrengthCommand& cmd, DirtSimStateMachine& dsm) {
    auto params = dsm.getSharedState().getPhysicsParams();
    params.dynamicStrength = cmd.strength;
    dsm.getSharedState().updatePhysicsParams(params);
    
    // Apply to world if it's WorldB.
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            if (world->getWorldType() == WorldType::RulesB) {
                world->setDynamicPressureStrength(cmd.strength);
            }
        }
    }
    
    spdlog::debug("SimRunning: Set dynamic strength to {}", cmd.strength);
    return *this;
}

// Handle immediate events routed through push system.
State::Any SimRunning::onEvent(const GetFPSCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    // FPS is already tracked in shared state and will be in next push update.
    spdlog::debug("SimRunning: GetFPSCommand - FPS will be in next update");
    
    // Force a push update with FPS dirty flag.
    if (dsm.getSharedState().isPushUpdatesEnabled()) {
        UIUpdateEvent update = dsm.buildUIUpdate();
        update.dirty.fps = true;
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }
    
    return *this;
}

State::Any SimRunning::onEvent(const GetSimStatsCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    // Stats are already tracked and will be in next push update.
    spdlog::debug("SimRunning: GetSimStatsCommand - Stats will be in next update");
    
    // Force a push update with stats dirty flag.
    if (dsm.getSharedState().isPushUpdatesEnabled()) {
        UIUpdateEvent update = dsm.buildUIUpdate();
        update.dirty.stats = true;
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }
    
    return *this;
}

State::Any SimRunning::onEvent(const ToggleDebugCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    // Toggle debug draw state.
    auto params = dsm.getSharedState().getPhysicsParams();
    params.debugEnabled = !params.debugEnabled;
    dsm.getSharedState().updatePhysicsParams(params);
    spdlog::debug("SimRunning: ToggleDebugCommand - Debug draw now: {}", params.debugEnabled);
    
    // Force a push update with uiState dirty flag.
    if (dsm.getSharedState().isPushUpdatesEnabled()) {
        UIUpdateEvent update = dsm.buildUIUpdate();
        update.dirty.uiState = true;
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }
    
    return *this;
}

State::Any SimRunning::onEvent(const ToggleForceCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    auto params = dsm.getSharedState().getPhysicsParams();
    params.forceVisualizationEnabled = !params.forceVisualizationEnabled;
    dsm.getSharedState().updatePhysicsParams(params);
    spdlog::debug("SimRunning: ToggleForceCommand - Force viz now: {}", params.forceVisualizationEnabled);
    
    // Force a push update with physics params dirty flag.
    if (dsm.getSharedState().isPushUpdatesEnabled()) {
        UIUpdateEvent update = dsm.buildUIUpdate();
        update.dirty.physicsParams = true;
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }
    
    return *this;
}

State::Any SimRunning::onEvent(const ToggleCohesionCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    auto params = dsm.getSharedState().getPhysicsParams();
    params.cohesionEnabled = !params.cohesionEnabled;
    dsm.getSharedState().updatePhysicsParams(params);
    spdlog::debug("SimRunning: ToggleCohesionCommand - Cohesion now: {}", params.cohesionEnabled);
    
    // Force a push update with physics params dirty flag.
    if (dsm.getSharedState().isPushUpdatesEnabled()) {
        UIUpdateEvent update = dsm.buildUIUpdate();
        update.dirty.physicsParams = true;
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }
    
    return *this;
}

State::Any SimRunning::onEvent(const ToggleAdhesionCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    auto params = dsm.getSharedState().getPhysicsParams();
    params.adhesionEnabled = !params.adhesionEnabled;
    dsm.getSharedState().updatePhysicsParams(params);
    
    // Update world if available.
    if (dsm.simulationManager && dsm.simulationManager->getWorld()) {
        dsm.simulationManager->getWorld()->setAdhesionEnabled(params.adhesionEnabled);
    }
    
    spdlog::debug("SimRunning: ToggleAdhesionCommand - Adhesion now: {}", params.adhesionEnabled);
    
    // Force a push update with physics params dirty flag.
    if (dsm.getSharedState().isPushUpdatesEnabled()) {
        UIUpdateEvent update = dsm.buildUIUpdate();
        update.dirty.physicsParams = true;
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }
    
    return *this;
}

State::Any SimRunning::onEvent(const ToggleTimeHistoryCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    auto params = dsm.getSharedState().getPhysicsParams();
    params.timeHistoryEnabled = !params.timeHistoryEnabled;
    dsm.getSharedState().updatePhysicsParams(params);
    spdlog::debug("SimRunning: ToggleTimeHistoryCommand - Time history now: {}", params.timeHistoryEnabled);
    
    // Force a push update with physics params dirty flag.
    if (dsm.getSharedState().isPushUpdatesEnabled()) {
        UIUpdateEvent update = dsm.buildUIUpdate();
        update.dirty.physicsParams = true;
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }
    
    return *this;
}

State::Any SimRunning::onEvent(const PrintAsciiDiagramCommand& /*cmd. */, DirtSimStateMachine& dsm) {
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

} // namespace State.
} // namespace DirtSim.