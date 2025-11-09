#pragma once

#include "core/WorldData.h"
#include <memory>

// Forward declarations.
typedef struct _lv_obj_t lv_obj_t;

namespace DirtSim {
namespace Ui {

// Forward declarations.
class UiComponentManager;
class CoreControls;
class SandboxControls;
class PhysicsControls;
class CellRenderer;
class WebSocketClient;
class EventSink;

/**
 * @brief Coordinates the simulation playground view.
 *
 * SimPlayground ties together all the UI components for the simulation:
 * - Core controls (quit, stats, debug)
 * - Scenario controls (sandbox toggles)
 * - Physics controls (parameter sliders)
 * - World renderer (cell grid)
 */
class SimPlayground {
public:
    SimPlayground(UiComponentManager* uiManager, WebSocketClient* wsClient, EventSink& eventSink);
    ~SimPlayground();

    /**
     * @brief Update UI from world data.
     */
    void updateFromWorldData(const WorldData& data);

    /**
     * @brief Render world state.
     */
    void render(const WorldData& data, bool debugDraw);

private:
    UiComponentManager* uiManager_;
    WebSocketClient* wsClient_;
    EventSink& eventSink_;

    // UI components.
    std::unique_ptr<CoreControls> coreControls_;
    std::unique_ptr<SandboxControls> sandboxControls_;
    std::unique_ptr<PhysicsControls> physicsControls_;
    std::unique_ptr<CellRenderer> renderer_;

    // Current scenario ID (to detect changes).
    std::string currentScenarioId_;
};

} // namespace Ui
} // namespace DirtSim
