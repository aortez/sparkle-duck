#include "SimPlayground.h"
#include "controls/CoreControls.h"
#include "controls/PhysicsControls.h"
#include "controls/SandboxControls.h"
#include "rendering/CellRenderer.h"
#include "rendering/NeuralGridRenderer.h"
#include "server/api/SimRun.h"
#include "state-machine/EventSink.h"
#include "state-machine/network/WebSocketClient.h"
#include "ui/UiComponentManager.h"
#include "ui/ui_builders/LVGLBuilder.h"
#include <cstring>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

SimPlayground::SimPlayground(
    UiComponentManager* uiManager, WebSocketClient* wsClient, EventSink& eventSink)
    : uiManager_(uiManager), wsClient_(wsClient), eventSink_(eventSink)
{
    // Create core controls in left panel.
    lv_obj_t* coreContainer = uiManager_->getCoreControlsContainer();
    coreControls_ =
        std::make_unique<CoreControls>(coreContainer, wsClient_, eventSink_, renderMode_);

    // Create physics controls in bottom panel.
    lv_obj_t* physicsContainer = uiManager_->getPhysicsControlsContainer();
    physicsControls_ = std::make_unique<PhysicsControls>(physicsContainer, wsClient_);

    // Create scenario dropdown in scenario controls container (persistent across scenarios).
    lv_obj_t* scenarioContainer = uiManager_->getScenarioControlsContainer();

    // Scenario label.
    lv_obj_t* scenarioLabel = lv_label_create(scenarioContainer);
    lv_label_set_text(scenarioLabel, "Scenario:");

    // Scenario dropdown.
    scenarioDropdown_ = LVGLBuilder::dropdown(scenarioContainer)
                            .options("Sandbox\nDam Break\nEmpty\nFalling Dirt\nRaining\nTree "
                                     "Germination\nWater Equalization")
                            .selected(0) // "Sandbox" selected by default.
                            .size(LV_PCT(90), 40)
                            .buildOrLog();

    if (scenarioDropdown_) {
        spdlog::info("SimPlayground: Scenario dropdown created successfully");

        // Style the dropdown button (light green background, dark purple text).
        lv_obj_set_style_bg_color(
            scenarioDropdown_, lv_color_hex(0x90EE90), LV_PART_MAIN); // Light green.
        lv_obj_set_style_text_color(
            scenarioDropdown_, lv_color_hex(0x4B0082), LV_PART_MAIN); // Dark purple (indigo).

        // Style the dropdown list (when opened).
        lv_obj_set_style_bg_color(
            lv_dropdown_get_list(scenarioDropdown_),
            lv_color_hex(0x90EE90),
            LV_PART_MAIN); // Light green.
        lv_obj_set_style_text_color(
            lv_dropdown_get_list(scenarioDropdown_),
            lv_color_hex(0x4B0082),
            LV_PART_MAIN); // Dark purple.

        lv_obj_set_user_data(scenarioDropdown_, this);
        lv_obj_add_event_cb(scenarioDropdown_, onScenarioChanged, LV_EVENT_VALUE_CHANGED, this);
    }
    else {
        spdlog::error("SimPlayground: Failed to create scenario dropdown!");
    }

    // Create cell renderer for world display.
    renderer_ = std::make_unique<CellRenderer>();

    // Create neural grid renderer for tree vision display.
    neuralGridRenderer_ = std::make_unique<NeuralGridRenderer>();

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

    // Always update controls with latest config (idempotent, detects changes internally).
    if (data.scenario_id == "sandbox" && sandboxControls_
        && std::holds_alternative<SandboxConfig>(data.scenario_config)) {
        const SandboxConfig& config = std::get<SandboxConfig>(data.scenario_config);
        sandboxControls_->updateFromConfig(config);
        sandboxControls_->updateWorldDimensions(data.width, data.height);
    }
}

void SimPlayground::render(const WorldData& data, bool debugDraw)
{
    lv_obj_t* worldContainer = uiManager_->getWorldDisplayArea();

    // Render world state (CellRenderer handles initialization/resize internally).
    renderer_->renderWorldData(data, worldContainer, debugDraw, renderMode_);
}

void SimPlayground::setRenderMode(RenderMode mode)
{
    renderMode_ = mode;

    // Sync dropdown to match.
    if (coreControls_) {
        coreControls_->setRenderMode(mode);
    }

    spdlog::info("SimPlayground: Render mode set to {}", renderModeToString(mode));
}

void SimPlayground::renderNeuralGrid(const WorldData& data)
{
    lv_obj_t* neuralGridContainer = uiManager_->getNeuralGridDisplayArea();

    // Adjust layout based on plant presence.
    if (data.tree_vision.has_value()) {
        // Plant exists: 50/50 split.
        uiManager_->setDisplayAreaRatio(1, 1);
        neuralGridRenderer_->renderSensoryData(data.tree_vision.value(), neuralGridContainer);
    }
    else {
        // No plant: 90/10 split (world gets more space).
        uiManager_->setDisplayAreaRatio(9, 1);
        neuralGridRenderer_->renderEmpty(neuralGridContainer);
    }
}

void SimPlayground::onScenarioChanged(lv_event_t* e)
{
    auto* playground = static_cast<SimPlayground*>(
        lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e))));
    if (!playground) return;

    lv_obj_t* dropdown = static_cast<lv_obj_t*>(lv_event_get_target(e));
    uint16_t selectedIdx = lv_dropdown_get_selected(dropdown);

    // Map dropdown index to scenario_id (must match dropdown options order).
    const char* scenarioIds[] = { "sandbox",           "dam_break", "empty",
                                  "falling_dirt",      "raining",   "tree_germination",
                                  "water_equalization" };

    constexpr size_t SCENARIO_COUNT = 7;
    if (selectedIdx >= SCENARIO_COUNT) {
        spdlog::error("SimPlayground: Invalid scenario index {}", selectedIdx);
        return;
    }

    std::string scenario_id = scenarioIds[selectedIdx];
    spdlog::info("SimPlayground: Scenario changed to '{}'", scenario_id);

    // Send sim_run command with new scenario_id to DSSM server.
    if (playground->wsClient_ && playground->wsClient_->isConnected()) {
        const DirtSim::Api::SimRun::Command cmd{ .timestep = 0.016,
                                                 .max_steps = -1,
                                                 .scenario_id = scenario_id,
                                                 .max_frame_ms = playground->currentMaxFrameMs_ };

        spdlog::info(
            "SimPlayground: Sending sim_run with scenario '{}', max_frame_ms={}",
            scenario_id,
            cmd.max_frame_ms);
        playground->wsClient_->sendCommand(cmd);
    }
    else {
        spdlog::warn("SimPlayground: WebSocket not connected, cannot switch scenario");
    }
}

std::optional<SimPlayground::ScreenshotData> SimPlayground::captureScreenshotPixels()
{
    if (!renderer_) {
        spdlog::error("SimPlayground: Cannot capture screenshot, renderer not initialized");
        return std::nullopt;
    }

    const uint8_t* buffer = renderer_->getCanvasBuffer();
    uint32_t width = renderer_->getCanvasWidth();
    uint32_t height = renderer_->getCanvasHeight();

    if (!buffer || width == 0 || height == 0) {
        spdlog::error("SimPlayground: Cannot capture screenshot, canvas not initialized");
        return std::nullopt;
    }

    // Calculate buffer size (ARGB8888 = 4 bytes per pixel).
    size_t bufferSize = static_cast<size_t>(width) * height * 4;

    // Make a copy of the pixel data.
    ScreenshotData data;
    data.width = width;
    data.height = height;
    data.pixels.resize(bufferSize);
    std::memcpy(data.pixels.data(), buffer, bufferSize);

    spdlog::info("SimPlayground: Captured screenshot {}x{} ({} bytes)", width, height, bufferSize);
    return data;
}

} // namespace Ui
} // namespace DirtSim
