#include "State.h"
#include "../DirtSimStateMachine.h"
#include "../WorldFactory.h"
#include "../WorldSetup.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace State {

// Extend Startup with event handlers.
State::Any Startup::onEvent(const InitCompleteEvent& /*evt. */, DirtSimStateMachine& dsm) {
    spdlog::info("Startup: Initialization complete, creating world");
    
    // Create default world (WorldB/RulesB)
    dsm.world = createWorld(WorldType::RulesB, 50, 50);
    
    // Initialize world with some default content.
    if (dsm.world) {
        // Could call dsm.world->setupInitialContent() or similar.
        spdlog::info("Startup: World created successfully");
    } else {
        spdlog::error("Startup: Failed to create world!");
        return Shutdown{};
    }
    
    // Transition to main menu.
    return MainMenu{};
}

} // namespace State.
} // namespace DirtSim.