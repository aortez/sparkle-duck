#pragma once

#include "Event.h"
#include <functional>
#include <lvgl/lvgl.h>

namespace DirtSim {

/**
 * @brief Factory methods to convert LVGL callbacks to Events.
 *
 * These methods bridge the gap between LVGL's C-style callbacks
 * and our type-safe event system.
 */
class LvglEventFactory {
public:
    // Callback data that gets passed through LVGL user_data
    struct CallbackContext {
        std::function<void(const Event&)> eventHandler;
        void* userData; // Additional user data if needed
    };

    /**
     * @brief Create callback for pause/resume button.
     * Generates PauseCommand or ResumeCommand based on current state.
     */
    static void pauseButtonCallback(lv_event_t* e);

    /**
     * @brief Create callback for reset button.
     * Generates ResetSimulationCommand.
     */
    static void resetButtonCallback(lv_event_t* e);

    /**
     * @brief Create callback for world type button matrix.
     * Generates SwitchWorldTypeCommand.
     */
    static void worldTypeButtonCallback(lv_event_t* e);

    /**
     * @brief Create callbacks for mouse events on draw area.
     * Generates MouseDownEvent, MouseMoveEvent, MouseUpEvent.
     */
    static void drawAreaMouseCallback(lv_event_t* e);

    /**
     * @brief Create callback for material selection.
     * Generates SelectMaterialCommand.
     */
    static void materialButtonCallback(lv_event_t* e);

    /**
     * @brief Create callback for gravity toggle.
     * Generates SetGravityCommand.
     */
    static void gravityButtonCallback(lv_event_t* e);

    /**
     * @brief Create callback for timescale slider.
     * Generates SetTimescaleCommand.
     */
    static void timescaleSliderCallback(lv_event_t* e);

    /**
     * @brief Create callback for elasticity slider.
     * Generates SetElasticityCommand.
     */
    static void elasticitySliderCallback(lv_event_t* e);

    /**
     * @brief Create callback for screenshot button.
     * Generates CaptureScreenshotCommand.
     */
    static void screenshotButtonCallback(lv_event_t* e);

    /**
     * @brief Create callback for quit button.
     * Generates QuitApplicationCommand.
     */
    static void quitButtonCallback(lv_event_t* e);

    /**
     * @brief Helper to extract event handler from LVGL user data.
     */
    static std::function<void(const Event&)>* getEventHandler(lv_event_t* e);

    /**
     * @brief Helper to convert LVGL coordinates to pixel coordinates.
     */
    static std::pair<int, int> getPixelCoordinates(lv_obj_t* obj, lv_point_t* point);
};

} // namespace DirtSim