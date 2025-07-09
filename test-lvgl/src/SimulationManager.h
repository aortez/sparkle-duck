#pragma once

#include "SimulatorUI.h"
#include "WorldFactory.h"
#include "WorldInterface.h"
#include "WorldState.h"
#include "lvgl/lvgl.h"

#include <cstdint>
#include <memory>

/**
 * @brief Central manager for simulation state and world switching.
 *
 * The SimulationManager owns both the world and UI components, providing
 * a clean separation of concerns and enabling headless operation.
 * It handles all world switching, state management, and coordinates
 * the relationship between UI and physics systems.
 */
class SimulationManager {
public:
    /**
     * @brief Construct a new SimulationManager.
     * @param initialType The initial world type to create
     * @param width Grid width in cells
     * @param height Grid height in cells
     * @param screen LVGL screen for UI creation (can be nullptr for headless)
     */
    SimulationManager(
        WorldType initialType, uint32_t width, uint32_t height, lv_obj_t* screen = nullptr);

    /**
     * @brief Destructor.
     */
    ~SimulationManager() = default;

    // =================================================================
    // CORE SIMULATION MANAGEMENT.
    // =================================================================

    /**
     * @brief Switch to a different world type.
     * @param newType The world type to switch to
     * @return true if switch was successful, false otherwise
     */
    bool switchWorldType(WorldType newType);

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
     * @brief Get the current world type.
     * @return The current world type
     */
    WorldType getCurrentWorldType() const;

    /**
     * @brief Get grid dimensions.
     */
    uint32_t getWidth() const { return width_; }
    uint32_t getHeight() const { return height_; }

    // =================================================================
    // STATE MANAGEMENT.
    // =================================================================

    /**
     * @brief Preserve current world state.
     * @param state Output state structure
     */
    void preserveState(WorldState& state) const;

    /**
     * @brief Restore world state.
     * @param state State to restore
     */
    void restoreState(const WorldState& state);

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

    uint32_t width_;               ///< Grid width in cells.
    uint32_t height_;              ///< Grid height in cells.
    uint32_t default_width_;       ///< Default grid width in cells.
    uint32_t default_height_;      ///< Default grid height in cells.
    WorldType initial_world_type_; ///< Initial world type to create.

    // =================================================================
    // PRIVATE METHODS.
    // =================================================================

    /**
     * @brief Create a new world of the specified type.
     * @param type World type to create
     * @return Unique pointer to new world instance
     */
    std::unique_ptr<WorldInterface> createWorld(WorldType type);

    /**
     * @brief Setup bidirectional relationship between UI and world.
     */
    void connectUIAndWorld();

    /**
     * @brief Update UI to reflect current world type.
     */
    void updateUIWorldType();
};