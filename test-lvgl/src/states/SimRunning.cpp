#include "State.h"
#include "../Cell.h"
#include "../CellB.h"
#include "../DirtSimStateMachine.h"
#include "../SimulationManager.h"
#include "../SimulatorUI.h"
#include "../WorldB.h"
#include "../WorldFactory.h"
#include "../WorldSetup.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace State {

void SimRunning::onEnter(DirtSimStateMachine& dsm) {
    spdlog::info("SimRunning: Entering simulation state");

    // SimulationManager is now created in DirtSimStateMachine constructor,
    // so we just need to use it here.
    if (!dsm.simulationManager) {
        spdlog::error("SimRunning: No SimulationManager available!");
        return;
    }

    // Initialize step count from shared state (preserves count when resuming from pause).
    stepCount = dsm.getSharedState().getCurrentStep();

    // Log whether we're starting fresh or resuming.
    if (stepCount == 0) {
        spdlog::info("SimRunning: Starting fresh simulation");
    } else {
        spdlog::info("SimRunning: Resuming simulation at step {}", stepCount);
    }

    spdlog::info("SimRunning: Ready to run simulation");
}

void SimRunning::onExit(DirtSimStateMachine& /*dsm. */) {
    spdlog::info("SimRunning: Exiting state");
    
    // Note: We don't destroy SimulationManager here anymore.
    // It will be destroyed by states that actually need to destroy it.
    // (like when transitioning to MainMenu or Shutdown).
    // SimPaused needs the SimulationManager to remain alive.
}


State::Any SimRunning::onEvent(const AdvanceSimulationCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    // NOTE: When in SimRunning state, the simulation is advanced by the UI thread's
    // processFrame loop (simulator_loop.h:83). We should NOT call advanceTime here
    // because that would create a race condition with two threads advancing physics
    // simultaneously. This handler exists for compatibility but is a no-op in SimRunning.
    // The UI loop handles step counting and stat updates automatically.

    spdlog::trace("SimRunning: AdvanceSimulationCommand received but ignored (UI loop drives simulation)");

    // Suppress unused parameter warning.
    (void)dsm;

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

State::Any SimRunning::onEvent(const StepBackwardCommand& /*cmd*/, DirtSimStateMachine& dsm) {
    spdlog::debug("SimRunning: Stepping simulation backward by one timestep");

    if (!dsm.simulationManager || !dsm.simulationManager->getWorld()) {
        spdlog::warn("SimRunning: Cannot step backward - no world available");
        return *this;
    }

    auto world = dsm.simulationManager->getWorld();

    // TODO: Implement world->goBackward() method for time reversal.
    (void)world;  // Silence unused variable warning until implemented.
    spdlog::info("StepBackwardCommand: Time reversal not yet implemented");

    return *this;  // Stay in SimRunning.
}

State::Any SimRunning::onEvent(const StepForwardCommand& /*cmd*/, DirtSimStateMachine& dsm) {
    if (!dsm.simulationManager || !dsm.simulationManager->getWorld()) {
        spdlog::warn("SimRunning: Cannot step forward - no world available");
        return *this;
    }

    auto world = dsm.simulationManager->getWorld();

    // TODO: Implement world->goForward() method for time reversal.
    (void)world;  // Silence unused variable warning until implemented.
    spdlog::info("SimRunning: Step forward requested");

    return *this;  // Stay in SimRunning.
}

State::Any SimRunning::onEvent(const ToggleTimeReversalCommand& /*cmd*/, DirtSimStateMachine& dsm) {
    if (!dsm.simulationManager || !dsm.simulationManager->getWorld()) {
        spdlog::warn("SimRunning: Cannot toggle time reversal - no world available");
        return *this;
    }

    auto world = dsm.simulationManager->getWorld();

    // TODO: Implement world->toggleTimeReversal() method.
    (void)world;  // Silence unused variable warning until implemented.
    spdlog::info("SimRunning: Toggle time reversal requested");

    return *this;  // Stay in SimRunning.
}

State::Any SimRunning::onEvent(const SetWaterCohesionCommand& cmd, DirtSimStateMachine& /*dsm*/) {
    Cell::setCohesionStrength(cmd.cohesion_value);
    spdlog::info("SimRunning: Set water cohesion to {}", cmd.cohesion_value);
    return *this;
}

State::Any SimRunning::onEvent(const SetWaterViscosityCommand& cmd, DirtSimStateMachine& /*dsm*/) {
    Cell::setViscosityFactor(cmd.viscosity_value);
    spdlog::info("SimRunning: Set water viscosity to {}", cmd.viscosity_value);
    return *this;
}

State::Any SimRunning::onEvent(const SetWaterPressureThresholdCommand& cmd, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            world->setWaterPressureThreshold(cmd.threshold_value);
            spdlog::info("SimRunning: Set water pressure threshold to {}", cmd.threshold_value);
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetWaterBuoyancyCommand& cmd, DirtSimStateMachine& /*dsm*/) {
    Cell::setBuoyancyStrength(cmd.buoyancy_value);
    spdlog::info("SimRunning: Set water buoyancy to {}", cmd.buoyancy_value);
    return *this;
}

State::Any SimRunning::onEvent(const LoadWorldCommand& cmd, DirtSimStateMachine& /*dsm*/) {
    Loading loadState;
    loadState.filepath = cmd.filepath;
    spdlog::info("SimRunning: Loading world from {}", cmd.filepath);
    return loadState;
}

State::Any SimRunning::onEvent(const SetTimestepCommand& cmd, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            // TODO: Implement world->setTimestep() method when available.
            (void)world;
        }
    }
    spdlog::debug("SimRunning: Set timestep to {}", cmd.timestep_value);
    return *this;
}

State::Any SimRunning::onEvent(const MouseDownEvent& evt, DirtSimStateMachine& dsm) {
    if (!dsm.simulationManager || !dsm.simulationManager->getWorld()) {
        return *this;
    }

    auto* world = dsm.simulationManager->getWorld();

    // Always enter GRAB_MODE - either grab existing material or create new and grab it.
    interactionMode = InteractionMode::GRAB_MODE;

    if (world->hasMaterialAtPixel(evt.pixelX, evt.pixelY)) {
        // Cell has material - grab it.
        world->startDragging(evt.pixelX, evt.pixelY);
        spdlog::debug("MouseDown: Grabbing existing material at ({}, {})", evt.pixelX, evt.pixelY);
    } else {
        // Cell is empty - add material first, then grab it.
        auto material = dsm.getSharedState().getSelectedMaterial();
        world->addMaterialAtPixel(evt.pixelX, evt.pixelY, material);
        world->startDragging(evt.pixelX, evt.pixelY);
        spdlog::debug("MouseDown: Creating and grabbing new {} at ({}, {})",
                      static_cast<int>(material), evt.pixelX, evt.pixelY);
    }

    return *this;
}

State::Any SimRunning::onEvent(const MouseMoveEvent& evt, DirtSimStateMachine& dsm) {
    if (!dsm.simulationManager || !dsm.simulationManager->getWorld()) {
        return *this;
    }

    auto* world = dsm.simulationManager->getWorld();

    // Only update drag position if we're in GRAB_MODE.
    // No more continuous painting in PAINT_MODE.
    if (interactionMode == InteractionMode::GRAB_MODE) {
        world->updateDrag(evt.pixelX, evt.pixelY);
    }

    return *this;
}

State::Any SimRunning::onEvent(const MouseUpEvent& evt, DirtSimStateMachine& dsm) {
    if (!dsm.simulationManager || !dsm.simulationManager->getWorld()) {
        return *this;
    }

    auto* world = dsm.simulationManager->getWorld();

    if (interactionMode == InteractionMode::GRAB_MODE) {
        // End dragging and release material with velocity.
        world->endDragging(evt.pixelX, evt.pixelY);
        spdlog::debug("MouseUp: Ending GRAB_MODE at ({}, {})", evt.pixelX, evt.pixelY);
    }

    // Reset interaction mode.
    interactionMode = InteractionMode::NONE;

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
    // Update world directly (source of truth).
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            world->setTimescale(cmd.timescale);
            spdlog::info("SimRunning: Set timescale to {}", cmd.timescale);
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetElasticityCommand& cmd, DirtSimStateMachine& dsm) {
    // Update world directly (source of truth).
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            world->setElasticityFactor(cmd.elasticity);
            spdlog::info("SimRunning: Set elasticity to {}", cmd.elasticity);
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetDynamicStrengthCommand& cmd, DirtSimStateMachine& dsm) {
    // Update world directly (source of truth).
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            if (world->getWorldType() == WorldType::RulesB) {
                world->setDynamicPressureStrength(cmd.strength);
                spdlog::info("SimRunning: Set dynamic strength to {:.1f}", cmd.strength);
            }
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetGravityCommand& cmd, DirtSimStateMachine& dsm) {
    // Update world directly (source of truth).
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            world->setGravity(cmd.gravity);
            spdlog::info("SimRunning: Set gravity to {}", cmd.gravity);
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetPressureScaleCommand& cmd, DirtSimStateMachine& dsm) {
    // Apply to world.
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            world->setPressureScale(cmd.scale);
        }
    }

    spdlog::debug("SimRunning: Set pressure scale to {}", cmd.scale);
    return *this;
}

State::Any SimRunning::onEvent(const SetPressureScaleWorldBCommand& cmd, DirtSimStateMachine& dsm) {
    // Apply to world.
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            world->setPressureScale(cmd.scale);
        }
    }

    spdlog::debug("SimRunning: Set WorldB pressure scale to {}", cmd.scale);
    return *this;
}

State::Any SimRunning::onEvent(const SetCohesionForceStrengthCommand& cmd, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            world->setCohesionComForceStrength(cmd.strength);
            spdlog::info("SimRunning: Set cohesion force strength to {}", cmd.strength);
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetAdhesionStrengthCommand& cmd, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            world->setAdhesionStrength(cmd.strength);
            spdlog::info("SimRunning: Set adhesion strength to {}", cmd.strength);
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetViscosityStrengthCommand& cmd, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            world->setViscosityStrength(cmd.strength);
            spdlog::info("SimRunning: Set viscosity strength to {}", cmd.strength);
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetFrictionStrengthCommand& cmd, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            world->setFrictionStrength(cmd.strength);
            spdlog::info("SimRunning: Set friction strength to {}", cmd.strength);
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetCOMCohesionRangeCommand& cmd, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            world->setCOMCohesionRange(cmd.range);
            spdlog::info("SimRunning: Set COM cohesion range to {}", cmd.range);
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetAirResistanceCommand& cmd, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            world->setAirResistanceStrength(cmd.strength);
            spdlog::info("SimRunning: Set air resistance to {}", cmd.strength);
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const ToggleHydrostaticPressureCommand& /*cmd*/, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            bool newValue = !world->isHydrostaticPressureEnabled();
            world->setHydrostaticPressureEnabled(newValue);
            spdlog::info("SimRunning: Toggle hydrostatic pressure - now: {}", newValue);
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const ToggleDynamicPressureCommand& /*cmd*/, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            bool newValue = !world->isDynamicPressureEnabled();
            world->setDynamicPressureEnabled(newValue);
            spdlog::info("SimRunning: Toggle dynamic pressure - now: {}", newValue);
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const TogglePressureDiffusionCommand& /*cmd*/, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            bool newValue = !world->isPressureDiffusionEnabled();
            world->setPressureDiffusionEnabled(newValue);
            spdlog::info("SimRunning: Toggle pressure diffusion - now: {}", newValue);
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetHydrostaticPressureStrengthCommand& cmd, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            world->setHydrostaticPressureStrength(cmd.strength);
            spdlog::info("SimRunning: Set hydrostatic pressure strength to {}", cmd.strength);
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetDynamicPressureStrengthCommand& cmd, DirtSimStateMachine& dsm) {
    // Apply to world.
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            // TODO: Need to add setDynamicPressureStrength method to WorldInterface.
            // For now, suppress unused warning.
            (void)world;
        }
    }

    spdlog::debug("SimRunning: Set dynamic pressure strength to {}", cmd.strength);
    return *this;
}

State::Any SimRunning::onEvent(const SetRainRateCommand& cmd, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            world->setRainRate(cmd.rate);
            spdlog::info("SimRunning: Set rain rate to {}", cmd.rate);
        }
    }
    return *this;
}

// Handle immediate events routed through push system.
State::Any SimRunning::onEvent(const GetFPSCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    // FPS is already tracked in shared state and will be in next push update.
    spdlog::debug("SimRunning: GetFPSCommand - FPS will be in next update");

    // Force a push update with FPS dirty flag.
    UIUpdateEvent update = dsm.buildUIUpdate();
    update.dirty.fps = true;
    dsm.getSharedState().pushUIUpdate(std::move(update));

    return *this;
}

State::Any SimRunning::onEvent(const GetSimStatsCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    // Stats are already tracked and will be in next push update.
    spdlog::debug("SimRunning: GetSimStatsCommand - Stats will be in next update");

    // Force a push update with stats dirty flag.
    UIUpdateEvent update = dsm.buildUIUpdate();
    update.dirty.stats = true;
    dsm.getSharedState().pushUIUpdate(std::move(update));

    return *this;
}

State::Any SimRunning::onEvent(const ToggleDebugCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    // Toggle debug draw state in world (source of truth).
    if (dsm.simulationManager && dsm.simulationManager->getWorld()) {
        auto* world = dsm.simulationManager->getWorld();
        bool newValue = !world->isDebugDrawEnabled();
        world->setDebugDrawEnabled(newValue);
        world->markAllCellsDirty();
        spdlog::info("SimRunning: ToggleDebugCommand - Debug draw now: {}", newValue);

        // Push UI update with uiState dirty flag.
        UIUpdateEvent update = dsm.buildUIUpdate();
        update.dirty.uiState = true;
        dsm.getSharedState().pushUIUpdate(std::move(update));
    }

    return *this;
}

State::Any SimRunning::onEvent(const ToggleCohesionForceCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            bool newValue = !world->isCohesionComForceEnabled();
            world->setCohesionComForceEnabled(newValue);
            spdlog::info("SimRunning: ToggleCohesionForceCommand - Cohesion force now: {}", newValue);

            UIUpdateEvent update = dsm.buildUIUpdate();
            update.dirty.uiState = true;
            dsm.getSharedState().pushUIUpdate(std::move(update));
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const ToggleTimeHistoryCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            bool newValue = !world->isTimeReversalEnabled();
            world->enableTimeReversal(newValue);
            spdlog::info("SimRunning: ToggleTimeHistoryCommand - Time history now: {}", newValue);

            UIUpdateEvent update = dsm.buildUIUpdate();
            update.dirty.uiState = true;
            dsm.getSharedState().pushUIUpdate(std::move(update));
        }
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

State::Any SimRunning::onEvent(const SpawnDirtBallCommand& /*cmd. */, DirtSimStateMachine& dsm) {
    // Get the current world and spawn a 5x5 ball at top center.
    if (dsm.simulationManager && dsm.simulationManager->getWorld()) {
        auto* world = dsm.simulationManager->getWorld();

        // Calculate the top center position.
        uint32_t centerX = world->getWidth() / 2;
        uint32_t topY = 2; // Start at row 2 to avoid the very top edge.

        // Spawn a 5x5 ball of the currently selected material.
        MaterialType selectedMaterial = world->getSelectedMaterial();
        world->spawnMaterialBall(selectedMaterial, centerX, topY, 2);
    }
    else {
        spdlog::warn("SpawnDirtBallCommand: No world available");
    }

    return *this;
}

State::Any SimRunning::onEvent(const SetFragmentationCommand& cmd, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            world->setDirtFragmentationFactor(cmd.factor);
            spdlog::info("SimRunning: Set fragmentation factor to {}", cmd.factor);
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const SetPressureSystemCommand& cmd, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            world->setPressureSystem(cmd.system);
            spdlog::info("SimRunning: Set pressure system to {}", static_cast<int>(cmd.system));
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const ToggleWallsCommand& /*cmd*/, DirtSimStateMachine& dsm) {
    // Apply to world.
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            // TODO: Need to add toggleWalls method to WorldInterface.
            // For now, suppress unused warning.
            (void)world;
        }
    }

    spdlog::info("SimRunning: Toggle walls");
    return *this;
}

State::Any SimRunning::onEvent(const ToggleWaterColumnCommand& /*cmd*/, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            bool newValue = !world->isWaterColumnEnabled();
            world->setWaterColumnEnabled(newValue);

            // For WorldB, we can manipulate cells directly.
            WorldB* worldB = dynamic_cast<WorldB*>(world);
            if (worldB) {
                if (newValue) {
                    // Add water column (5 wide × 20 tall) on left side.
                    spdlog::info("SimRunning: Adding water column (5 wide × 20 tall) at runtime");
                    for (uint32_t y = 0; y < 20 && y < worldB->getHeight(); ++y) {
                        for (uint32_t x = 1; x <= 5 && x < worldB->getWidth(); ++x) {
                            CellB& cell = worldB->at(x, y);
                            // Only add water to non-wall cells.
                            if (!cell.isWall()) {
                                cell.setMaterialType(MaterialType::WATER);
                                cell.setFillRatio(1.0);
                                cell.setCOM(Vector2d(0.0, 0.0));
                                cell.setVelocity(Vector2d(0.0, 0.0));
                                cell.markDirty();
                            }
                        }
                    }
                } else {
                    // Remove water from column area (only water cells).
                    spdlog::info("SimRunning: Removing water from water column area at runtime");
                    for (uint32_t y = 0; y < 20 && y < worldB->getHeight(); ++y) {
                        for (uint32_t x = 1; x <= 5 && x < worldB->getWidth(); ++x) {
                            CellB& cell = worldB->at(x, y);
                            // Only clear water cells, leave walls and other materials.
                            if (cell.getMaterialType() == MaterialType::WATER && !cell.isWall()) {
                                cell.setMaterialType(MaterialType::AIR);
                                cell.setFillRatio(0.0);
                                cell.setCOM(Vector2d(0.0, 0.0));
                                cell.setVelocity(Vector2d(0.0, 0.0));
                                cell.markDirty();
                            }
                        }
                    }
                }
            }

            spdlog::info("SimRunning: Water column toggled - now: {}", newValue);
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const ToggleLeftThrowCommand& /*cmd*/, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            bool newValue = !world->isLeftThrowEnabled();
            world->setLeftThrowEnabled(newValue);
            spdlog::info("SimRunning: Toggle left throw - now: {}", newValue);
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const ToggleRightThrowCommand& /*cmd*/, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            bool newValue = !world->isRightThrowEnabled();
            world->setRightThrowEnabled(newValue);
            spdlog::info("SimRunning: Toggle right throw - now: {}", newValue);
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const ToggleQuadrantCommand& /*cmd*/, DirtSimStateMachine& dsm) {
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            bool newValue = !world->isLowerRightQuadrantEnabled();
            world->setLowerRightQuadrantEnabled(newValue);

            // For WorldB, manipulate cells directly for immediate feedback.
            WorldB* worldB = dynamic_cast<WorldB*>(world);
            if (worldB) {
                uint32_t startX = worldB->getWidth() / 2;
                uint32_t startY = worldB->getHeight() / 2;

                if (newValue) {
                    // Add dirt quadrant immediately.
                    spdlog::info("SimRunning: Adding lower right quadrant ({}x{}) at runtime",
                                 worldB->getWidth() - startX, worldB->getHeight() - startY);
                    for (uint32_t y = startY; y < worldB->getHeight(); ++y) {
                        for (uint32_t x = startX; x < worldB->getWidth(); ++x) {
                            CellB& cell = worldB->at(x, y);
                            // Only add dirt to non-wall cells.
                            if (!cell.isWall()) {
                                cell.setMaterialType(MaterialType::DIRT);
                                cell.setFillRatio(1.0);
                                cell.setCOM(Vector2d(0.0, 0.0));
                                cell.setVelocity(Vector2d(0.0, 0.0));
                                cell.markDirty();
                            }
                        }
                    }
                } else {
                    // Remove dirt from quadrant area (only dirt cells).
                    spdlog::info("SimRunning: Removing dirt from lower right quadrant at runtime");
                    for (uint32_t y = startY; y < worldB->getHeight(); ++y) {
                        for (uint32_t x = startX; x < worldB->getWidth(); ++x) {
                            CellB& cell = worldB->at(x, y);
                            // Only clear dirt cells, leave walls and other materials.
                            if (cell.getMaterialType() == MaterialType::DIRT && !cell.isWall()) {
                                cell.setMaterialType(MaterialType::AIR);
                                cell.setFillRatio(0.0);
                                cell.setCOM(Vector2d(0.0, 0.0));
                                cell.setVelocity(Vector2d(0.0, 0.0));
                                cell.markDirty();
                            }
                        }
                    }
                }
            }

            spdlog::info("SimRunning: Toggle quadrant - now: {}", newValue);
        }
    }
    return *this;
}

State::Any SimRunning::onEvent(const ToggleFrameLimitCommand& /*cmd*/, DirtSimStateMachine& dsm) {
    // Apply to world.
    if (auto* simMgr = dsm.getSimulationManager()) {
        if (auto* world = simMgr->getWorld()) {
            // TODO: Need to add toggleFrameLimit method to WorldInterface.
            // For now, suppress unused warning.
            (void)world;
        }
    }

    spdlog::info("SimRunning: Toggle frame limit");
    return *this;
}

State::Any SimRunning::onEvent(const QuitApplicationCommand& /*cmd*/, DirtSimStateMachine& /*dsm*/) {
    spdlog::info("SimRunning: Quit application requested");

    // Take exit screenshot before quitting.
    SimulatorUI::takeExitScreenshot();

    // Transition to Shutdown state.
    return Shutdown{};
}

} // namespace State.
} // namespace DirtSim.