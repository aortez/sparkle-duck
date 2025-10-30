#pragma once

#include "SimulatorUI.h"
#include "WorldInterface.h"
#include "lvgl/lvgl.h"

#include <cstdint>
#include <memory>

// Forward declarations
class EventRouter;

/**
 * @brief Central manager for simulation state.
 *
 * The SimulationManager owns both the world and UI components, providing
 * a clean separation of concerns and enabling headless operation.
 * It coordinates the relationship between UI and physics systems.
 */
class SimulationManager {
public:
    /**
     * @brief Construct a new SimulationManager.
     * @param width Grid width in cells
     * @param height Grid height in cells
     * @param screen LVGL screen for UI creation (can be nullptr for headless)
     * @param eventRouter EventRouter for UI event handling (required for event system)
     */
    SimulationManager(
        uint32_t width,
        uint32_t height,
        lv_obj_t* screen,
        EventRouter* eventRouter);

    /**
     * @brief Destructor.
     */
    ~SimulationManager() = default;

    // =================================================================
    // CORE SIMULATION MANAGEMENT.
    // =================================================================

    /**
     * @brief Resize the world if necessary for a scenario.
     * @param requiredWidth The required width (0 = no requirement)
     * @param requiredHeight The required height (0 = no requirement)
     * @return true if resize was needed and successful, false if no resize needed
     */
    bool resizeWorldIfNeeded(uint32_t requiredWidth, uint32_t requiredHeight);

    /**
     * @brief Reset the current world to initial state.
     */
    void reset();

    /**
     * @brief Advance simulation by given time step.
     * @param deltaTime Time step in seconds
     */
    void advanceTime(double deltaTime);

    /**
     * @brief Check if the application should exit.
     * @return True if exit has been requested, false otherwise
     */
    bool shouldExit() const;

    /**
     * @brief Draw the current world state.
     */
    void draw();

    /**
     * @brief Initialize the simulation (call after construction).
     */
    void initialize();

    // =================================================================
    // ACCESSORS.
    // =================================================================

    /**
     * @brief Get the current world instance.
     * @return Raw pointer to world (never null after construction)
     */
    WorldInterface* getWorld() const { return world_.get(); }

    /**
     * @brief Get the UI instance.
     * @return Raw pointer to UI (null in headless mode)
     */
    SimulatorUI* getUI() const { return ui_.get(); }

    /**
     * @brief Check if running in headless mode.
     * @return true if no UI is present
     */
    bool isHeadless() const { return ui_ == nullptr; }

    /**
     * @brief Get grid dimensions.
     */
    uint32_t getWidth() const { return width_; }
    uint32_t getHeight() const { return height_; }

    /**
     * @brief Dump performance statistics.
     */
    void dumpTimerStats() const;

private:
    // =================================================================
    // MEMBER VARIABLES.
    // =================================================================

    std::unique_ptr<WorldInterface> world_; ///< Current world instance.
    std::unique_ptr<SimulatorUI> ui_;       ///< UI instance (null in headless mode).
    lv_obj_t* draw_area_;                   ///< LVGL draw area (from UI).
    EventRouter* eventRouter_;              ///< EventRouter for event handling (may be null).

    uint32_t width_;         ///< Grid width in cells.
    uint32_t height_;        ///< Grid height in cells.
    uint32_t default_width_; ///< Default grid width in cells.
    uint32_t default_height_; ///< Default grid height in cells.

    // =================================================================
    // PRIVATE METHODS.
    // =================================================================

    /**
     * @brief Setup bidirectional relationship between UI and world.
     */
    void connectUIAndWorld();
};