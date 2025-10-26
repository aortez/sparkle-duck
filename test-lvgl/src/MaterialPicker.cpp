#include "MaterialPicker.h"
#include "CellB.h"
#include "Event.h"
#include "EventRouter.h"
#include "SimulatorUI.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstring>

MaterialPicker::MaterialPicker(lv_obj_t* parent, EventRouter* eventRouter)
    : parent_(parent),
      material_grid_(nullptr),
      selected_material_(MaterialType::DIRT), // Default to DIRT as specified.
      parent_ui_(nullptr),
      event_router_(eventRouter)
{
    spdlog::debug("Creating MaterialPicker with default selection: DIRT");

    // Initialize button and canvas arrays to nullptr.
    material_buttons_.fill(nullptr);
    material_canvases_.fill(nullptr);
}

MaterialPicker::~MaterialPicker()
{
    spdlog::debug("Destroying MaterialPicker");

    // LVGL objects are automatically cleaned up when parent is destroyed.
    // but we could explicitly clean up here if needed.
}

// =================================================================
// UI CREATION AND MANAGEMENT
// =================================================================

void MaterialPicker::createMaterialSelector()
{
    spdlog::info("Creating material selector UI with {}x{} grid", GRID_COLS, GRID_ROWS);

    // Create main grid container.
    material_grid_ = lv_obj_create(parent_);
    lv_obj_set_size(material_grid_, calculatePickerWidth(), calculatePickerHeight());
    lv_obj_set_style_pad_all(material_grid_, 0, 0);
    lv_obj_set_style_border_width(material_grid_, 1, 0);
    lv_obj_set_style_border_color(material_grid_, lv_color_hex(0x808080), 0);

    // Set grid layout - this creates a flexible grid.
    lv_obj_set_layout(material_grid_, LV_LAYOUT_GRID);
    lv_obj_set_style_grid_column_dsc_array(
        material_grid_, (const lv_coord_t[]){ BUTTON_SIZE, BUTTON_SIZE, LV_GRID_TEMPLATE_LAST }, 0);
    lv_obj_set_style_grid_row_dsc_array(
        material_grid_,
        (const lv_coord_t[]){
            BUTTON_SIZE, BUTTON_SIZE, BUTTON_SIZE, BUTTON_SIZE, LV_GRID_TEMPLATE_LAST },
        0);
    lv_obj_set_style_grid_column_align(material_grid_, LV_GRID_ALIGN_SPACE_EVENLY, 0);
    lv_obj_set_style_grid_row_align(material_grid_, LV_GRID_ALIGN_SPACE_EVENLY, 0);

    // Create buttons for each material in the layout order.
    for (int row = 0; row < GRID_ROWS; row++) {
        for (int col = 0; col < GRID_COLS; col++) {
            int index = row * GRID_COLS + col;
            if (index < TOTAL_MATERIALS) {
                MaterialType material = MATERIAL_LAYOUT[index];
                createMaterialButton(material, col, row);
            }
        }
    }

    // Set initial selection highlighting.
    updateButtonHighlight(selected_material_);

    spdlog::info("Material selector created with {} buttons", TOTAL_MATERIALS);
}

void MaterialPicker::createMaterialButton(MaterialType type, int gridX, int gridY)
{
    int index = gridY * GRID_COLS + gridX;

    spdlog::trace(
        "Creating material button for {} at grid position ({},{}), index {}",
        getMaterialName(type),
        gridX,
        gridY,
        index);

    // Create button.
    lv_obj_t* button = lv_btn_create(material_grid_);
    lv_obj_set_size(button, BUTTON_SIZE, BUTTON_SIZE);
    lv_obj_set_grid_cell(button, LV_GRID_ALIGN_CENTER, gridX, 1, LV_GRID_ALIGN_CENTER, gridY, 1);

    // Store button reference.
    material_buttons_[index] = button;

    // Create material icon.
    createMaterialIcon(button, type);

    // Set up event handling - store material type as user data.
    lv_obj_set_user_data(button, reinterpret_cast<void*>(static_cast<uintptr_t>(type)));
    lv_obj_add_event_cb(button, onMaterialButtonClicked, LV_EVENT_CLICKED, this);

    // Configure button appearance.
    lv_obj_set_style_radius(button, 4, 0);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(0x404040), 0);

    spdlog::trace(
        "Created material button for {} with user_data={}",
        getMaterialName(type),
        static_cast<int>(type));
}

// =================================================================
// MATERIAL SELECTION
// =================================================================

void MaterialPicker::setSelectedMaterial(MaterialType type)
{
    if (selected_material_ != type) {
        spdlog::debug(
            "Material selection changed: {} -> {}",
            getMaterialName(selected_material_),
            getMaterialName(type));

        selected_material_ = type;
        updateButtonHighlight(type);
    }
}

// =================================================================
// EVENT HANDLING
// =================================================================

void MaterialPicker::onMaterialButtonClicked(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    lv_obj_t* button = static_cast<lv_obj_t*>(lv_event_get_target(e));
    MaterialPicker* picker = static_cast<MaterialPicker*>(lv_event_get_user_data(e));

    // Extract material type from button user data.
    uintptr_t materialData = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(button));
    MaterialType clickedMaterial = static_cast<MaterialType>(materialData);

    spdlog::info(
        "Material button clicked: {} (raw data: {})",
        getMaterialName(clickedMaterial),
        static_cast<int>(clickedMaterial));

    // Update selection.
    picker->setSelectedMaterial(clickedMaterial);

    // Route material selection through event system.
    if (picker->event_router_) {
        picker->event_router_->routeEvent(Event{SelectMaterialCommand{clickedMaterial}});
    }
    // Legacy callback for backward compatibility.
    else if (picker->parent_ui_) {
        picker->parent_ui_->onMaterialSelectionChanged(clickedMaterial);
    }
}

// =================================================================
// VISUAL CUSTOMIZATION
// =================================================================

void MaterialPicker::updateButtonHighlight(MaterialType selectedType)
{
    spdlog::trace(
        "Updating button highlights for selected material: {}", getMaterialName(selectedType));

    // Update all buttons to show/hide selection highlight.
    for (int i = 0; i < TOTAL_MATERIALS; i++) {
        if (material_buttons_[i] != nullptr) {
            MaterialType buttonMaterial = MATERIAL_LAYOUT[i];

            if (buttonMaterial == selectedType) {
                // Highlight selected button.
                lv_obj_set_style_border_color(material_buttons_[i], lv_color_hex(0x00FF00), 0);
                lv_obj_set_style_border_width(material_buttons_[i], 3, 0);
                lv_obj_set_style_bg_color(material_buttons_[i], lv_color_hex(0x2A2A2A), 0);
                spdlog::trace("Highlighted button for {}", getMaterialName(buttonMaterial));
            }
            else {
                // Normal button appearance.
                lv_obj_set_style_border_color(material_buttons_[i], lv_color_hex(0x404040), 0);
                lv_obj_set_style_border_width(material_buttons_[i], 2, 0);
                lv_obj_set_style_bg_color(material_buttons_[i], lv_color_hex(0x1A1A1A), 0);
            }
        }
    }
}

void MaterialPicker::createMaterialIcon(lv_obj_t* button, MaterialType type)
{
    spdlog::trace("Creating material icon for {}", getMaterialName(type));

    // For now, create a simple colored rectangle as material icon.
    // TODO: Integrate with CellB rendering system for consistent visuals.

    // Create label as material icon (simpler than canvas for now).
    lv_obj_t* icon = lv_label_create(button);
    lv_obj_set_size(icon, ICON_SIZE, ICON_SIZE);
    lv_obj_center(icon);

    // Set background color based on material.
    lv_color_t materialColor = getMaterialDisplayColor(type);
    lv_obj_set_style_bg_color(icon, materialColor, 0);
    lv_obj_set_style_bg_opa(icon, LV_OPA_80, 0);
    lv_obj_set_style_radius(icon, 4, 0);
    lv_obj_set_style_border_width(icon, 1, 0);
    lv_obj_set_style_border_color(icon, lv_color_white(), 0);
    lv_obj_set_style_border_opa(icon, LV_OPA_50, 0);

    // Set material name as text (first letter).
    const char* materialName = getMaterialName(type);
    char iconText[2] = { materialName[0], '\0' };
    lv_label_set_text(icon, iconText);
    lv_obj_set_style_text_color(icon, lv_color_white(), 0);
    lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, 0);

    // Calculate material index for storage.
    int materialIndex = 0;
    for (int i = 0; i < TOTAL_MATERIALS; i++) {
        if (MATERIAL_LAYOUT[i] == type) {
            materialIndex = i;
            break;
        }
    }

    // Store icon reference for potential future updates.
    material_canvases_[materialIndex] = icon;

    spdlog::trace(
        "Created {}x{} icon for {} with color and text '{}'",
        ICON_SIZE,
        ICON_SIZE,
        getMaterialName(type),
        iconText);
}

// =================================================================
// HELPER METHODS
// =================================================================

bool MaterialPicker::getMaterialGridPosition(MaterialType type, int& gridX, int& gridY) const
{
    for (int i = 0; i < TOTAL_MATERIALS; i++) {
        if (MATERIAL_LAYOUT[i] == type) {
            gridX = i % GRID_COLS;
            gridY = i / GRID_COLS;
            return true;
        }
    }
    return false;
}

MaterialType MaterialPicker::getMaterialFromGridPosition(int gridX, int gridY) const
{
    int index = gridY * GRID_COLS + gridX;
    if (index >= 0 && index < TOTAL_MATERIALS) {
        return MATERIAL_LAYOUT[index];
    }
    return MaterialType::AIR;
}

int MaterialPicker::calculatePickerWidth() const
{
    return (GRID_COLS * BUTTON_SIZE) + ((GRID_COLS - 1) * GRID_SPACING) + (2 * GRID_SPACING);
}

int MaterialPicker::calculatePickerHeight() const
{
    return (GRID_ROWS * BUTTON_SIZE) + ((GRID_ROWS - 1) * GRID_SPACING) + (2 * GRID_SPACING);
}

// =================================================================
// MATERIAL COLOR MAPPING (TEMPORARY)
// =================================================================

/**
 * Get display color for material type.
 * TODO: This should eventually use CellB's actual rendering system.
 */
lv_color_t MaterialPicker::getMaterialDisplayColor(MaterialType type)
{
    // Use the same enhanced colors as CellB for consistency.
    switch (type) {
        case MaterialType::DIRT:
            return lv_color_hex(0x8B4513); // Rich saddle brown.
        case MaterialType::WATER:
            return lv_color_hex(0x1E90FF); // Dodger blue (more vibrant).
        case MaterialType::WOOD:
            return lv_color_hex(0xD2691E); // Chocolate brown (warmer wood tone).
        case MaterialType::SAND:
            return lv_color_hex(0xF4A460); // Sandy brown.
        case MaterialType::METAL:
            return lv_color_hex(0xB0C4DE); // Light steel blue (more metallic).
        case MaterialType::LEAF:
            return lv_color_hex(0x32CD32); // Lime green (brighter, more vibrant).
        case MaterialType::WALL:
            return lv_color_hex(0x696969); // Dim gray (darker, more solid).
        case MaterialType::AIR:
            return lv_color_hex(0x000000); // Black.
        default:
            return lv_color_hex(0xFF00FF); // Magenta for unknown.
    }
}