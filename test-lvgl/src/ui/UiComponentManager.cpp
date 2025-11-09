#include "UiComponentManager.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

UiComponentManager::UiComponentManager(lv_disp_t* display) : display(display)
{
    if (!display) {
        spdlog::error("UiComponentManager initialized with null display");
        return;
    }

    // Initialize with the default screen.
    currentScreen = lv_disp_get_scr_act(display);
    spdlog::info("UiComponentManager initialized with display");
}

UiComponentManager::~UiComponentManager()
{
    spdlog::info("UiComponentManager cleanup started");

    // Clean up any screens we created (not the default one).
    if (simulationScreen && simulationScreen != lv_disp_get_scr_act(display)) {
        cleanupScreen(simulationScreen);
    }
    if (mainMenuScreen && mainMenuScreen != lv_disp_get_scr_act(display)) {
        cleanupScreen(mainMenuScreen);
    }
    if (configScreen && configScreen != lv_disp_get_scr_act(display)) {
        cleanupScreen(configScreen);
    }

    spdlog::info("UiComponentManager cleanup completed");
}

lv_obj_t* UiComponentManager::getSimulationContainer()
{
    if (!display) return nullptr;

    simulationScreen = ensureScreen(simulationScreen, "simulation");
    transitionToScreen(simulationScreen);

    // Create layout structure if not already created.
    if (!simTopRow_) {
        createSimulationLayout();
    }

    return simulationScreen;
}

lv_obj_t* UiComponentManager::getCoreControlsContainer()
{
    getSimulationContainer(); // Ensure layout is created.
    return simCoreControlsArea_;
}

lv_obj_t* UiComponentManager::getScenarioControlsContainer()
{
    getSimulationContainer(); // Ensure layout is created.
    return simScenarioControlsArea_;
}

lv_obj_t* UiComponentManager::getPhysicsControlsContainer()
{
    getSimulationContainer(); // Ensure layout is created.
    return simPhysicsControlsArea_;
}

lv_obj_t* UiComponentManager::getWorldDisplayArea()
{
    getSimulationContainer(); // Ensure layout is created.
    return simWorldDisplayArea_;
}

lv_obj_t* UiComponentManager::getMainMenuContainer()
{
    if (!display) return nullptr;

    mainMenuScreen = ensureScreen(mainMenuScreen, "main_menu");
    transitionToScreen(mainMenuScreen);
    return mainMenuScreen;
}

lv_obj_t* UiComponentManager::getConfigContainer()
{
    if (!display) return nullptr;

    configScreen = ensureScreen(configScreen, "config");
    transitionToScreen(configScreen);
    return configScreen;
}

void UiComponentManager::clearCurrentContainer()
{
    if (currentScreen) {
        lv_obj_clean(currentScreen);
        spdlog::debug("Cleared current container");
    }
}

void UiComponentManager::transitionToScreen(lv_obj_t* screen, bool animate)
{
    if (!screen || screen == currentScreen) {
        return;
    }

    if (animate) {
        lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
    }
    else {
        lv_scr_load(screen);
    }

    currentScreen = screen;
    spdlog::debug("Transitioned to screen");
}

lv_obj_t* UiComponentManager::ensureScreen(lv_obj_t*& screen, const char* name)
{
    if (!screen) {
        screen = lv_obj_create(NULL);
        if (screen) {
            spdlog::debug("Created {} screen", name);
        }
        else {
            spdlog::error("Failed to create {} screen", name);
        }
    }
    return screen;
}

void UiComponentManager::cleanupScreen(lv_obj_t*& screen)
{
    if (screen) {
        lv_obj_del(screen);
        screen = nullptr;
        spdlog::debug("Cleaned up screen");
    }
}

void UiComponentManager::createSimulationLayout()
{
    if (!simulationScreen) {
        spdlog::error("createSimulationLayout: simulation screen not created");
        return;
    }

    // Main container with vertical flex (top row, then bottom panel).
    lv_obj_set_flex_flow(simulationScreen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        simulationScreen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(simulationScreen, 0, 0);
    lv_obj_set_style_pad_gap(simulationScreen, 0, 0);

    // Top row: left panel + world display (horizontal layout, grows to fill space above bottom
    // panel).
    simTopRow_ = lv_obj_create(simulationScreen);
    lv_obj_set_width(simTopRow_, LV_PCT(100));
    lv_obj_set_flex_grow(simTopRow_, 1); // Grow to fill vertical space.
    lv_obj_set_flex_flow(simTopRow_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        simTopRow_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(simTopRow_, 0, 0);
    lv_obj_set_style_pad_gap(simTopRow_, 0, 0);
    lv_obj_set_style_border_width(simTopRow_, 0, 0);
    lv_obj_set_style_bg_opa(simTopRow_, LV_OPA_TRANSP, 0);

    // Left panel (260px wide, fills full height of top row).
    simLeftPanel_ = lv_obj_create(simTopRow_);
    lv_obj_set_size(simLeftPanel_, 260, LV_PCT(100));
    lv_obj_set_flex_flow(simLeftPanel_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        simLeftPanel_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(simLeftPanel_, 2, 0);
    lv_obj_set_style_pad_all(simLeftPanel_, 5, 0);
    lv_obj_set_scroll_dir(simLeftPanel_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(simLeftPanel_, LV_SCROLLBAR_MODE_AUTO);

    // Core controls area (within left panel).
    simCoreControlsArea_ = lv_obj_create(simLeftPanel_);
    lv_obj_set_size(simCoreControlsArea_, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(simCoreControlsArea_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(simCoreControlsArea_, 0, 0);
    lv_obj_set_style_border_width(simCoreControlsArea_, 0, 0);
    lv_obj_set_style_bg_opa(simCoreControlsArea_, LV_OPA_TRANSP, 0);

    // Scenario controls area (within left panel, below core).
    simScenarioControlsArea_ = lv_obj_create(simLeftPanel_);
    lv_obj_set_size(simScenarioControlsArea_, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(simScenarioControlsArea_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(simScenarioControlsArea_, 0, 0);
    lv_obj_set_style_border_width(simScenarioControlsArea_, 0, 0);
    lv_obj_set_style_bg_opa(simScenarioControlsArea_, LV_OPA_TRANSP, 0);

    // World display area (flex-grow to fill remaining horizontal and vertical space).
    simWorldDisplayArea_ = lv_obj_create(simTopRow_);
    lv_obj_set_size(simWorldDisplayArea_, LV_PCT(100), LV_PCT(100)); // Fill parent.
    lv_obj_set_flex_grow(simWorldDisplayArea_, 1);
    lv_obj_set_style_pad_all(simWorldDisplayArea_, 0, 0);
    lv_obj_set_style_border_width(simWorldDisplayArea_, 0, 0);
    lv_obj_set_style_bg_opa(simWorldDisplayArea_, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(simWorldDisplayArea_, LV_OBJ_FLAG_SCROLLABLE); // Disable scrolling.

    // CellRenderer uses absolute positioning and calculates centering offset.

    // Bottom panel: physics controls (3-column horizontal layout).
    simBottomPanel_ = lv_obj_create(simulationScreen);
    lv_obj_set_size(simBottomPanel_, LV_PCT(100), 200);
    lv_obj_set_flex_flow(simBottomPanel_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        simBottomPanel_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(simBottomPanel_, 5, 0);
    lv_obj_set_style_pad_gap(simBottomPanel_, 10, 0);
    lv_obj_set_scroll_dir(simBottomPanel_, LV_DIR_HOR);

    // Physics controls area (the bottom panel itself, for now - could subdivide into 3 columns).
    simPhysicsControlsArea_ = simBottomPanel_;

    spdlog::info("UiComponentManager: Created simulation layout structure");
}

} // namespace Ui
} // namespace DirtSim