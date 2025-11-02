#pragma once

#include "../core/MaterialType.h"
#include "lvgl/lvgl.h"

#include <array>
#include <memory>

// Forward declarations.
class EventRouter;
class SimulatorUI;

/**
 * \file
 * MaterialPicker provides a visual UI for selecting from all 8 material types.
 * Uses a 4x2 grid layout with mini-cell icons for each material type.
 * Integrates with SimulationManager for material selection state management.
 */

class MaterialPicker {
public:
    // Button layout constants.
    static constexpr int GRID_ROWS = 4;
    static constexpr int GRID_COLS = 2;
    static constexpr int TOTAL_MATERIALS = 8;
    static constexpr int BUTTON_SIZE = 64; // 64x64px buttons.
    static constexpr int ICON_SIZE = 32;   // 32x32px material icons.
    static constexpr int GRID_SPACING = 8; // Spacing between buttons.

    // Material layout order (left-to-right, top-to-bottom in 4×2 grid).
    static constexpr std::array<MaterialType, TOTAL_MATERIALS> MATERIAL_LAYOUT = {
        { MaterialType::DIRT,
          MaterialType::WATER,
          MaterialType::SAND,
          MaterialType::WOOD,
          MaterialType::METAL,
          MaterialType::LEAF,
          MaterialType::WALL,
          MaterialType::AIR }
    };

    /**
     * Constructor - creates material picker UI within parent container.
     * @param parent LVGL parent object to contain the material picker.
     * @param eventRouter EventRouter for sending material selection events (optional for legacy).
     */
    explicit MaterialPicker(lv_obj_t* parent, EventRouter* eventRouter = nullptr);

    /**
     * Destructor - cleanup LVGL objects and resources.
     */
    ~MaterialPicker();

    // =================================================================
    // UI CREATION AND MANAGEMENT
    // =================================================================

    /**
     * Create the complete material selector UI.
     * - Sets up 2x4 button grid
     * - Creates material icons for each button
     * - Configures event handlers
     */
    void createMaterialSelector();

    /**
     * Create individual material button with icon.
     * @param type Material type for this button.
     * @param gridX Grid X position (0-3).
     * @param gridY Grid Y position (0-1).
     */
    void createMaterialButton(MaterialType type, int gridX, int gridY);

    // =================================================================
    // MATERIAL SELECTION
    // =================================================================

    /**
     * Get currently selected material.
     * @return Currently selected MaterialType.
     */
    MaterialType getSelectedMaterial() const { return selected_material_; }

    /**
     * Set selected material and update UI highlighting.
     * @param type Material type to select.
     */
    void setSelectedMaterial(MaterialType type);

    /**
     * Set parent UI for material selection notifications.
     * @param ui Pointer to parent SimulatorUI.
     */
    void setParentUI(SimulatorUI* ui) { parent_ui_ = ui; }

    // =================================================================
    // EVENT HANDLING
    // =================================================================

    /**
     * Static callback for material button clicks.
     * @param e LVGL event containing button and user data.
     */
    static void onMaterialButtonClicked(lv_event_t* e);

    // =================================================================
    // VISUAL CUSTOMIZATION
    // =================================================================

    /**
     * Update button highlighting to show selected material.
     * @param selectedType Material type to highlight (or AIR for none).
     */
    void updateButtonHighlight(MaterialType selectedType);

    /**
     * Create material icon for button using mini-cell rendering.
     * @param button LVGL button object to add icon to.
     * @param type Material type to render.
     */
    void createMaterialIcon(lv_obj_t* button, MaterialType type);

private:
    // UI components.
    lv_obj_t* parent_;                                         // Parent container.
    lv_obj_t* material_grid_;                                  // Main grid container.
    std::array<lv_obj_t*, TOTAL_MATERIALS> material_buttons_;  // Material selection buttons.
    std::array<lv_obj_t*, TOTAL_MATERIALS> material_canvases_; // Icon canvases for buttons.

    // State management.
    MaterialType selected_material_; // Currently selected material.
    SimulatorUI* parent_ui_;         // Parent UI for notifications (legacy).
    EventRouter* event_router_;      // EventRouter for material selection events.

    // =================================================================
    // HELPER METHODS
    // =================================================================

    /**
     * Get grid position for a material type.
     * @param type Material type to find.
     * @param gridX Output: X position in grid (0-3).
     * @param gridY Output: Y position in grid (0-1).
     * @return true if material found in layout.
     */
    bool getMaterialGridPosition(MaterialType type, int& gridX, int& gridY) const;

    /**
     * Get material type from grid position.
     * @param gridX X position in grid (0-3).
     * @param gridY Y position in grid (0-1).
     * @return MaterialType at that position.
     */
    MaterialType getMaterialFromGridPosition(int gridX, int gridY) const;

    /**
     * Calculate total picker width based on layout.
     * @return Total width in pixels needed for the picker.
     */
    int calculatePickerWidth() const;

    /**
     * Calculate total picker height based on layout.
     * @return Total height in pixels needed for the picker.
     */
    int calculatePickerHeight() const;

    /**
     * Get display color for material type (temporary implementation).
     * TODO: Replace with actual Cell rendering integration
     * @param type Material type to get color for.
     * @return LVGL color for the material.
     */
    static lv_color_t getMaterialDisplayColor(MaterialType type);
};