#include "State.h"
#include "../DirtSimStateMachine.h"
#include "../SimulationManager.h"
#include "../SimulatorUI.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace State {

void SimPaused::onEnter(DirtSimStateMachine& dsm) {
    spdlog::info("SimPaused: Entered pause state from SimRunning at step {}", previousState.stepCount);
    
    // Set the pause flag
    dsm.getSharedState().setIsPaused(true);
    
    // Update pause button state if UI exists
    if (dsm.simulationManager && dsm.simulationManager->getUI()) {
        // TODO: Update pause button visual state
    }
}

void SimPaused::onExit(DirtSimStateMachine& dsm) {
    spdlog::info("SimPaused: Exiting pause state");
    
    // Clear the pause flag
    dsm.getSharedState().setIsPaused(false);
}

State::Any SimPaused::onEvent(const ResumeCommand& /*cmd*/, DirtSimStateMachine& /*dsm*/) {
    spdlog::info("SimPaused: Resuming to SimRunning at step {}", previousState.stepCount);
    
    return std::move(previousState);
}

State::Any SimPaused::onEvent(const ResetSimulationCommand& /*cmd*/, DirtSimStateMachine& dsm) {
    spdlog::info("SimPaused: Resetting simulation");
    
    if (dsm.simulationManager) {
        dsm.simulationManager->reset();
    }
    
    // Return to running state with reset count
    return SimRunning{};
}

State::Any SimPaused::onEvent(const AdvanceSimulationCommand& /*cmd*/, DirtSimStateMachine& dsm) {
    // In paused state, we can still advance one step (for frame-by-frame debugging)
    if (dsm.simulationManager) {
        dsm.simulationManager->advanceTime(1.0/60.0);  // Single step
        previousState.stepCount++;
        dsm.getSharedState().setCurrentStep(previousState.stepCount);
        
        spdlog::debug("SimPaused: Advanced one step to {}", previousState.stepCount);
    }
    
    return *this;  // Stay paused
}

State::Any SimPaused::onEvent(const MouseDownEvent& evt, DirtSimStateMachine& dsm) {
    // Allow material placement while paused
    if (dsm.simulationManager && dsm.simulationManager->getWorld()) {
        auto material = dsm.getSharedState().getSelectedMaterial();
        dsm.simulationManager->getWorld()->addMaterialAtPixel(evt.pixelX, evt.pixelY, material);
    }
    
    return *this;
}

State::Any SimPaused::onEvent(const SelectMaterialCommand& cmd, DirtSimStateMachine& dsm) {
    dsm.getSharedState().setSelectedMaterial(cmd.material);
    return *this;
}

} // namespace State
} // namespace DirtSim