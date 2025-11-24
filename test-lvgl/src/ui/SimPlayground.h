#pragma once

#include "core/WorldData.h"
#include "ui/rendering/RenderMode.h"
#include <memory>

// Forward declarations.
typedef struct _lv_obj_t lv_obj_t;
typedef struct _lv_event_t lv_event_t;

namespace DirtSim {
namespace Ui {

// Forward declarations.
class UiComponentManager;
class CoreControls;
class SandboxControls;
class PhysicsControls;
class CellRenderer;
class NeuralGridRenderer;
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
     * @param uiFPS Current UI frame rate for display.
     */
    void updateFromWorldData(const WorldData& data, double uiFPS = 0.0);

    /**
     * @brief Render world state.
     */
    void render(const WorldData& data, bool debugDraw);

    /**
     * @brief Set render mode and update UI dropdown.
     */
    void setRenderMode(RenderMode mode);

    /**
     * @brief Get current render mode.
     */
    RenderMode getRenderMode() const { return renderMode_; }

    /**
     * @brief Render neural grid (tree vision).
     * @param data World data containing tree information.
     */
    void renderNeuralGrid(const WorldData& data);

    /**
     * @brief Get physics controls for settings updates.
     */
    PhysicsControls* getPhysicsControls() { return physicsControls_.get(); }

private:
    UiComponentManager* uiManager_;
    RenderMode renderMode_ = RenderMode::ADAPTIVE; // Persists across scenario changes.
    WebSocketClient* wsClient_;
    EventSink& eventSink_;

    // UI components.
    std::unique_ptr<CoreControls> coreControls_;
    std::unique_ptr<SandboxControls> sandboxControls_;
    std::unique_ptr<PhysicsControls> physicsControls_;
    std::unique_ptr<CellRenderer> renderer_;
    std::unique_ptr<NeuralGridRenderer> neuralGridRenderer_;

    // Scenario selector dropdown (persistent across scenario changes).
    lv_obj_t* scenarioDropdown_ = nullptr;

    // Current scenario ID (to detect changes).
    std::string currentScenarioId_;

    // Current scenario config (to detect changes).
    ScenarioConfig currentScenarioConfig_;

    // Current frame limit.
    int currentMaxFrameMs_ = 16;

    // Event handlers.
    static void onScenarioChanged(lv_event_t* e);
};

} // namespace Ui
} // namespace DirtSim
