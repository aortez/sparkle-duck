#include "SimPlayground.h"
#include "controls/CoreControls.h"
#include "controls/PhysicsControls.h"
#include "controls/SandboxControls.h"
#include "rendering/CellRenderer.h"
#include "state-machine/EventSink.h"
#include "state-machine/network/WebSocketClient.h"
#include "ui/UiComponentManager.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

SimPlayground::SimPlayground(
    UiComponentManager* uiManager, WebSocketClient* wsClient, EventSink& eventSink)
    : uiManager_(uiManager), wsClient_(wsClient), eventSink_(eventSink)
{
    // Create core controls in left panel.
    lv_obj_t* coreContainer = uiManager_->getCoreControlsContainer();
    coreControls_ = std::make_unique<CoreControls>(coreContainer, wsClient_, eventSink_);

    // Create physics controls in bottom panel.
    lv_obj_t* physicsContainer = uiManager_->getPhysicsControlsContainer();
    physicsControls_ = std::make_unique<PhysicsControls>(physicsContainer, wsClient_);

    // Create cell renderer for world display.
    renderer_ = std::make_unique<CellRenderer>();

    spdlog::info("SimPlayground: Initialized");
}

SimPlayground::~SimPlayground()
{
    spdlog::info("SimPlayground: Destroyed");
}

void SimPlayground::updateFromWorldData(const WorldData& data, double uiFPS)
{
    // Update stats display.
    coreControls_->updateStats(data.fps_server, uiFPS);

    // Handle scenario changes.
    if (data.scenario_id != currentScenarioId_) {
        spdlog::info("SimPlayground: Scenario changed to '{}'", data.scenario_id);

        // Clear old scenario controls.
        sandboxControls_.reset();

        // Create new scenario controls based on scenario type.
        if (data.scenario_id == "sandbox") {
            lv_obj_t* scenarioContainer = uiManager_->getScenarioControlsContainer();
            const SandboxConfig& config = std::get<SandboxConfig>(data.scenario_config);
            sandboxControls_ =
                std::make_unique<SandboxControls>(scenarioContainer, wsClient_, config);
        }
        // TODO: Add other scenario control creators here.

        currentScenarioId_ = data.scenario_id;
    }
}

void SimPlayground::render(const WorldData& data, bool debugDraw)
{
    lv_obj_t* worldContainer = uiManager_->getWorldDisplayArea();

    // Render world state (CellRenderer handles initialization/resize internally).
    renderer_->renderWorldData(data, worldContainer, debugDraw);
}

} // namespace Ui
} // namespace DirtSim
