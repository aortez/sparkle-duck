#pragma once

#include "core/PhysicsSettings.h"
#include "lvgl/lvgl.h"
#include <functional>
#include <unordered_map>
#include <vector>

namespace DirtSim {
namespace Ui {

// Forward declarations.
class WebSocketClient;

/**
 * @brief Physics parameter controls for tuning simulation behavior.
 *
 * Provides toggle sliders for: timescale, gravity, elasticity, air resistance,
 * pressure systems (hydrostatic, dynamic, diffusion), and material forces
 * (cohesion, adhesion, viscosity, friction).
 *
 * Refactored to use data-driven approach with generic callbacks to reduce
 * repetition and improve maintainability.
 */
class PhysicsControls {
public:
    PhysicsControls(lv_obj_t* container, WebSocketClient* wsClient);
    ~PhysicsControls();

    /**
     * @brief Update all UI controls from server PhysicsSettings.
     */
    void updateFromSettings(const PhysicsSettings& settings);

private:
    // Control type enumeration.
    enum class ControlType { TOGGLE_SLIDER, SWITCH_ONLY };

    // Configuration for a single control.
    struct ControlConfig {
        const char* label;
        ControlType type;

        // Slider configuration (only used for TOGGLE_SLIDER).
        int rangeMin = 0;
        int rangeMax = 100;
        int defaultValue = 50;
        double valueScale = 1.0;
        const char* valueFormat = "%.1f";
        bool initiallyEnabled = false;

        // Which field in PhysicsSettings this control modifies.
        std::function<void(PhysicsSettings&, double)> valueSetter = nullptr;
        std::function<double(const PhysicsSettings&)> valueGetter = nullptr;
        std::function<void(PhysicsSettings&, bool)> enableSetter = nullptr;
        std::function<bool(const PhysicsSettings&)> enableGetter = nullptr;
    };

    // Column configuration.
    struct ColumnConfig {
        const char* title;
        std::vector<ControlConfig> controls;
    };

    // Generic control structure to track runtime objects.
    struct Control {
        ControlConfig config;
        lv_obj_t* widget = nullptr;       // Container for toggle slider or switch.
        lv_obj_t* switchWidget = nullptr; // The actual switch component.
        lv_obj_t* sliderWidget = nullptr; // The actual slider component (if applicable).
    };

    lv_obj_t* container_;
    WebSocketClient* wsClient_;

    // Current physics settings (local copy, synced with server).
    PhysicsSettings settings_;

    // Column containers.
    std::vector<lv_obj_t*> columns_;

    // All controls in a single vector.
    std::vector<Control> controls_;

    // Control lookup for fast access from widgets.
    std::unordered_map<lv_obj_t*, Control*> widgetToControl_;

    // Static control definitions - this replaces all the repetitive control creation.
    static std::vector<ColumnConfig> createColumnConfigs();

    // Setup methods.
    lv_obj_t* createColumn(const char* title);
    lv_obj_t* createCollapsibleColumn(const char* title);
    void createControlWidget(lv_obj_t* column, Control& control);

    // Generic event handlers (replaces 18+ static callbacks).
    static void onGenericToggle(lv_event_t* e);
    static void onGenericValueChange(lv_event_t* e);

    // Helper to find control from widget.
    Control* findControl(lv_obj_t* widget);

    /**
     * @brief Fetch current physics settings from server.
     */
    void fetchSettings();

    /**
     * @brief Send updated physics settings to server.
     */
    void syncSettings();
};

} // namespace Ui
} // namespace DirtSim
