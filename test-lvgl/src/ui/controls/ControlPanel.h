#pragma once

#include "core/ScenarioConfig.h"
#include "core/WorldData.h"
#include "lvgl/lvgl.h"
#include <memory>
#include <string>

namespace DirtSim {
namespace Ui {

// Forward declarations.
class WebSocketClient;
class EventSink;

/**
 * @brief Manages UI controls for simulation interaction.
 *
 * ControlPanel creates LVGL widgets for controlling the simulation:
 * - Core controls (Quit, Debug) - always present
 * - Scenario-specific controls - created based on active scenario
 *
 * Sends commands to DSSM server via WebSocketClient.
 * Queues UI-local commands to EventSink.
 */
class ControlPanel {
public:
    /**
     * @brief Create control panel.
     * @param container Parent LVGL container for controls.
     * @param wsClient WebSocket client for sending commands to DSSM.
     * @param eventSink Event sink for UI-local commands.
     */
    ControlPanel(lv_obj_t* container, WebSocketClient* wsClient, EventSink& eventSink);

    /**
     * @brief Destructor - cleans up LVGL widgets.
     */
    ~ControlPanel();

    /**
     * @brief Update controls based on world state.
     * @param data Current world data from DSSM.
     *
     * Updates widget states to match server state (debug toggle, etc.)
     * and rebuilds scenario controls if scenario changed.
     */
    void updateFromWorldData(const WorldData& data);

private:
    lv_obj_t* container_;
    WebSocketClient* wsClient_;
    EventSink& eventSink_;
    std::string currentScenarioId_;
    uint32_t worldWidth_ = 28;  // Current world width (updated from WorldData).
    uint32_t worldHeight_ = 28; // Current world height (updated from WorldData).

    // Control panel layout.
    lv_obj_t* panelContainer_ = nullptr;

    // Core controls (always present).
    lv_obj_t* quitButton_ = nullptr;
    lv_obj_t* debugSwitch_ = nullptr;

    // Scenario-specific controls container.
    lv_obj_t* scenarioPanel_ = nullptr;

    // Sandbox scenario controls.
    lv_obj_t* sandboxAddSeedButton_ = nullptr;
    lv_obj_t* sandboxQuadrantSwitch_ = nullptr;
    lv_obj_t* sandboxRainSlider_ = nullptr;
    lv_obj_t* sandboxRightThrowSwitch_ = nullptr;
    lv_obj_t* sandboxDropDirtBallButton_ = nullptr;
    lv_obj_t* sandboxWaterColumnSwitch_ = nullptr;

    /**
     * @brief Create core controls (Quit, Debug).
     */
    void createCoreControls();

    /**
     * @brief Create scenario-specific controls.
     * @param scenarioId Scenario ID (e.g., "sandbox").
     * @param config Scenario configuration.
     */
    void createScenarioControls(const std::string& scenarioId, const ScenarioConfig& config);

    /**
     * @brief Clear scenario-specific controls.
     */
    void clearScenarioControls();

    /**
     * @brief Create sandbox scenario controls.
     * @param config Sandbox configuration.
     */
    void createSandboxControls(const SandboxConfig& config);

    // Event handlers (static for LVGL callbacks).
    static void onAddSeedClicked(lv_event_t* e);
    static void onDebugToggled(lv_event_t* e);
    static void onDropDirtBallClicked(lv_event_t* e);
    static void onQuitClicked(lv_event_t* e);
    static void onSandboxQuadrantToggled(lv_event_t* e);
    static void onSandboxRainSliderChanged(lv_event_t* e);
    static void onSandboxRightThrowToggled(lv_event_t* e);
    static void onSandboxWaterColumnToggled(lv_event_t* e);

    /**
     * @brief Send scenario config update to DSSM.
     * @param config Updated configuration.
     */
    void sendConfigUpdate(const ScenarioConfig& config);

    /**
     * @brief Send debug toggle update to DSSM.
     * @param enabled Debug draw enabled state.
     */
    void sendDebugUpdate(bool enabled);
};

} // namespace Ui
} // namespace DirtSim
