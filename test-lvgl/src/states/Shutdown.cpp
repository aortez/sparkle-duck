#include "State.h"
#include "../DirtSimStateMachine.h"
#include "../WorldSetup.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace State {

void Shutdown::onEnter(DirtSimStateMachine& dsm) {
    spdlog::info("Shutdown: Performing cleanup");
    
    // UI cleanup happens automatically when states exit (they own their UI)
    // Just clean up UIManager screens.
    if (dsm.uiManager) {
        dsm.uiManager->clearCurrentContainer();
    }
    
    // Clean up SimulationManager (which owns the world)
    if (dsm.simulationManager) {
        dsm.simulationManager.reset();
    }
    
    // Clean up world (if it exists separately from SimulationManager)
    if (dsm.world) {
        // Could save state here.
        dsm.world.reset();
    }
    
    // Set exit flag.
    dsm.getSharedState().setShouldExit(true);
    
    spdlog::info("Shutdown: Cleanup complete");
}

} // namespace State.
} // namespace DirtSim.