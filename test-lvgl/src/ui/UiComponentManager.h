#pragma once

#include <lvgl.h>
#include <memory>
#include <string>

namespace DirtSim {

/**
 * @brief Lightweight manager for LVGL resources and screen management.
 *
 * UiComponentManager handles LVGL-specific resources like screens and containers,
 * but does NOT own business logic UI components. States own their UI
 * components and use UiComponentManager to get appropriate containers.
 */
class UiComponentManager {
public:
    explicit UiComponentManager(lv_disp_t* display);

    ~UiComponentManager();

    /**
     * @brief Get container for simulation UI.
     * Creates/prepares the simulation screen if needed.
     */
    lv_obj_t* getSimulationContainer();

    /**
     * @brief Get container for core controls (quit, stats, debug).
     * Creates layout on simulation screen if needed.
     */
    lv_obj_t* getCoreControlsContainer();

    /**
     * @brief Get container for scenario-specific controls.
     * Creates layout on simulation screen if needed.
     */
    lv_obj_t* getScenarioControlsContainer();

    /**
     * @brief Get container for physics parameter controls (3-column bottom panel).
     * Creates layout on simulation screen if needed.
     */
    lv_obj_t* getPhysicsControlsContainer();

    /**
     * @brief Get container for world display area (canvas grid).
     * Creates layout on simulation screen if needed.
     */
    lv_obj_t* getWorldDisplayArea();

    /**
     * @brief Get container for main menu UI.
     * Creates/prepares the menu screen if needed.
     */
    lv_obj_t* getMainMenuContainer();

    /**
     * @brief Get container for configuration UI.
     * Creates/prepares the config screen if needed.
     */
    lv_obj_t* getConfigContainer();

    /**
     * @brief Clear the current container of all children.
     * Called when states exit to ensure clean transitions.
     */
    void clearCurrentContainer();

    /**
     * @brief Get the current active screen.
     */
    lv_obj_t* getCurrentScreen() const { return currentScreen; }

    /**
     * @brief Transition to a specific screen with optional animation.
     */
    void transitionToScreen(lv_obj_t* screen, bool animate = true);

private:
    lv_disp_t* display;

    // Screens for different states.
    lv_obj_t* simulationScreen = nullptr;
    lv_obj_t* mainMenuScreen = nullptr;
    lv_obj_t* configScreen = nullptr;

    // Current active screen.
    lv_obj_t* currentScreen = nullptr;

    // Simulation screen layout containers (created lazily).
    lv_obj_t* simTopRow_ = nullptr;
    lv_obj_t* simLeftPanel_ = nullptr;
    lv_obj_t* simCoreControlsArea_ = nullptr;
    lv_obj_t* simScenarioControlsArea_ = nullptr;
    lv_obj_t* simWorldDisplayArea_ = nullptr;
    lv_obj_t* simBottomPanel_ = nullptr;
    lv_obj_t* simPhysicsControlsArea_ = nullptr;

    /**
     * @brief Create a screen if it doesn't exist.
     */
    lv_obj_t* ensureScreen(lv_obj_t*& screen, const char* name);

    /**
     * @brief Clean up a screen and its children.
     */
    void cleanupScreen(lv_obj_t*& screen);

    /**
     * @brief Create the simulation screen layout structure.
     * Called lazily when first simulation container is requested.
     */
    void createSimulationLayout();
};

} // namespace DirtSim
