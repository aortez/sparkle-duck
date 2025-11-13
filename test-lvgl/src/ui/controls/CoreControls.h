#pragma once

#include "core/WorldData.h"
#include "lvgl/lvgl.h"

namespace DirtSim {
namespace Ui {

// Forward declarations.
class WebSocketClient;
class EventSink;

/**
 * @brief Core controls always present in simulation view.
 *
 * Includes: Quit button, Reset button, FPS stats display, Debug Draw toggle, World Size slider.
 */
class CoreControls {
public:
    CoreControls(lv_obj_t* container, WebSocketClient* wsClient, EventSink& eventSink);
    ~CoreControls();

    /**
     * @brief Update stats display with FPS values.
     */
    void updateStats(double serverFPS, double uiFPS);

private:
    lv_obj_t* container_;
    WebSocketClient* wsClient_;
    EventSink& eventSink_;

    // Widgets.
    lv_obj_t* quitButton_ = nullptr;
    lv_obj_t* resetButton_ = nullptr;
    lv_obj_t* statsLabel_ = nullptr;
    lv_obj_t* statsLabelUI_ = nullptr;
    lv_obj_t* debugSwitch_ = nullptr;
    lv_obj_t* pixelRendererSwitch_ = nullptr;
    lv_obj_t* worldSizeContainer_ = nullptr;
    lv_obj_t* worldSizeSwitch_ = nullptr;
    lv_obj_t* worldSizeSlider_ = nullptr;

    // State for throttling world size changes
    int32_t pendingWorldSize_ = 0;

    // Event handlers.
    static void onQuitClicked(lv_event_t* e);
    static void onResetClicked(lv_event_t* e);
    static void onDebugToggled(lv_event_t* e);
    static void onPixelRendererToggled(lv_event_t* e);
    static void onWorldSizeToggled(lv_event_t* e);
    static void onWorldSizeChanged(lv_event_t* e);
};

} // namespace Ui
} // namespace DirtSim
