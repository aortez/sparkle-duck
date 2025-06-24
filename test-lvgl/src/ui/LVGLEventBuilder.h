#pragma once

#include "LVGLBuilder.h"
#include "../Event.h"
#include "../EventRouter.h"
#include <memory>
#include <functional>
#include <optional>
#include <vector>

namespace DirtSim {

/**
 * @brief Extensions to LVGLBuilder for event handling with the event system.
 * 
 * These extensions allow LVGL widgets to emit events into our type-safe
 * event system using lambda callbacks.
 */
class LVGLEventBuilder {
public:
    // Forward declarations
    class SliderBuilder;
    class ButtonBuilder;
    class ButtonMatrixBuilder;
    class DropdownBuilder;
    
    /**
     * @brief Create a slider with event routing support.
     */
    static SliderBuilder slider(lv_obj_t* parent, EventRouter* router);
    
    /**
     * @brief Create a button with event routing support.
     */
    static ButtonBuilder button(lv_obj_t* parent, EventRouter* router);
    
    /**
     * @brief Create a button matrix with event routing support.
     */
    static ButtonMatrixBuilder buttonMatrix(lv_obj_t* parent, EventRouter* router);
    
    /**
     * @brief Create a dropdown with event routing support.
     */
    static DropdownBuilder dropdown(lv_obj_t* parent, EventRouter* router);
    /**
     * @brief Extended SliderBuilder with event system integration.
     */
    class SliderBuilder : public LVGLBuilder::SliderBuilder {
    public:
        using LVGLBuilder::SliderBuilder::SliderBuilder;
        
        /**
         * @brief Set up slider to emit events on value change.
         * @param handler Function that takes the slider value and returns an Event
         */
        SliderBuilder& onValueChange(std::function<Event(int32_t)> handler);
        
        /**
         * @brief Convenience method for timescale sliders.
         * Automatically converts slider value (0-100) to timescale (0.0-1.0)
         */
        SliderBuilder& onTimescaleChange();
        
        /**
         * @brief Convenience method for elasticity sliders.
         * Automatically converts slider value (0-100) to elasticity (0.0-1.0)
         */
        SliderBuilder& onElasticityChange();
        
        /**
         * @brief Convenience method for dynamic strength sliders.
         * Automatically converts slider value (0-300) to strength (0.0-3.0)
         */
        SliderBuilder& onDynamicStrengthChange();
        
        /**
         * @brief Set the event router for this builder.
         */
        SliderBuilder& withEventRouter(EventRouter* router);
        
    private:
        EventRouter* eventRouter_ = nullptr;
        std::shared_ptr<std::function<Event(int32_t)>> eventHandler_;
    };
    
    /**
     * @brief Extended ButtonBuilder with event system integration.
     */
    class ButtonBuilder : public LVGLBuilder::ButtonBuilder {
    public:
        using LVGLBuilder::ButtonBuilder::ButtonBuilder;
        
        /**
         * @brief Set up button to emit event on click.
         * @param event The event to emit when clicked
         */
        ButtonBuilder& onClick(Event event);
        
        /**
         * @brief Set up button to emit event on click with dynamic generation.
         * @param handler Function that generates the event when clicked
         */
        ButtonBuilder& onClick(std::function<Event()> handler);
        
        /**
         * @brief Set up toggle button to emit events based on state.
         * @param checkedEvent Event when button becomes checked
         * @param uncheckedEvent Event when button becomes unchecked
         */
        ButtonBuilder& onToggle(Event checkedEvent, Event uncheckedEvent);
        
        /**
         * @brief Convenience method for pause/resume buttons.
         */
        ButtonBuilder& onPauseResume();
        
        /**
         * @brief Convenience method for reset button.
         */
        ButtonBuilder& onReset();
        
        /**
         * @brief Convenience method for gravity toggle button.
         */
        ButtonBuilder& onGravityToggle();

        /**
         * @brief Convenience method for print ASCII button.
         */
        ButtonBuilder& onPrintAscii();
        
        /**
         * @brief Convenience method for debug toggle button.
         */
        ButtonBuilder& onDebugToggle();
        
        /**
         * @brief Convenience method for quit button.
         */
        ButtonBuilder& onQuit();
        
        /**
         * @brief Convenience method for screenshot button.
         */
        ButtonBuilder& onScreenshot();
        
        /**
         * @brief Convenience method for force toggle button.
         */
        ButtonBuilder& onForceToggle();
        
        /**
         * @brief Convenience method for cohesion toggle button.
         */
        ButtonBuilder& onCohesionToggle();
        
        /**
         * @brief Convenience method for adhesion toggle button.
         */
        ButtonBuilder& onAdhesionToggle();
        
        /**
         * @brief Convenience method for time history toggle button.
         */
        ButtonBuilder& onTimeHistoryToggle();
        
        /**
         * @brief Set the event router for this builder.
         */
        ButtonBuilder& withEventRouter(EventRouter* router);
        
    private:
        EventRouter* eventRouter_ = nullptr;
        std::shared_ptr<std::function<Event()>> eventHandler_;
        std::shared_ptr<std::pair<Event, Event>> toggleEvents_;
    };
    
    /**
     * @brief Extended button matrix builder for event handling.
     */
    class ButtonMatrixBuilder {
    public:
        explicit ButtonMatrixBuilder(lv_obj_t* parent) : parent_(parent), btnMatrix_(nullptr) {}
        
        /**
         * @brief Set up button matrix to emit events on selection.
         * @param handler Function that takes button index and returns an Event
         */
        ButtonMatrixBuilder& onSelect(std::function<Event(uint32_t)> handler);
        
        /**
         * @brief Convenience method for world type selection.
         * Maps button indices to WorldType enum.
         */
        ButtonMatrixBuilder& onWorldTypeSelect();
        
        /**
         * @brief Set the event router for this builder.
         */
        ButtonMatrixBuilder& withEventRouter(EventRouter* router);
        
        // Builder methods for configuration
        ButtonMatrixBuilder& map(const char** btnMap);
        ButtonMatrixBuilder& size(int width, int height);
        ButtonMatrixBuilder& position(int x, int y, lv_align_t align = LV_ALIGN_TOP_LEFT);
        ButtonMatrixBuilder& oneChecked(bool enable);
        ButtonMatrixBuilder& buttonCtrl(uint16_t btnId, lv_buttonmatrix_ctrl_t ctrl);
        ButtonMatrixBuilder& selectedButton(uint16_t btnId);
        ButtonMatrixBuilder& style(lv_style_selector_t selector, std::function<void(lv_style_t*)> styleFunc);
        
        Result<lv_obj_t*, std::string> build();
        lv_obj_t* buildOrLog();
        lv_obj_t* getButtonMatrix() const { return btnMatrix_; }
        
    private:
        lv_obj_t* parent_;
        lv_obj_t* btnMatrix_;
        EventRouter* eventRouter_ = nullptr;
        std::shared_ptr<std::function<Event(uint32_t)>> eventHandler_;
        
        // Configuration storage
        const char** btnMap_ = nullptr;
        std::optional<std::pair<int, int>> size_;
        std::optional<std::tuple<int, int, lv_align_t>> position_;
        bool oneChecked_ = false;
        std::vector<std::pair<uint16_t, lv_buttonmatrix_ctrl_t>> buttonCtrls_;
        std::optional<uint16_t> selectedButton_;
        std::vector<std::pair<lv_style_selector_t, std::function<void(lv_style_t*)>>> styles_;
    };
    
    /**
     * @brief Extended dropdown builder for event handling.
     */
    class DropdownBuilder : public LVGLBuilder::DropdownBuilder {
    public:
        using LVGLBuilder::DropdownBuilder::DropdownBuilder;
        
        /**
         * @brief Set up dropdown to emit events on value change.
         * @param handler Function that takes the selected index and returns an Event
         */
        DropdownBuilder& onValueChange(std::function<Event(uint16_t)> handler);
        
        /**
         * @brief Convenience method for pressure system dropdown.
         * Maps indices to pressure system types.
         */
        DropdownBuilder& onPressureSystemChange();
        
        /**
         * @brief Set the event router for this builder.
         */
        DropdownBuilder& withEventRouter(EventRouter* router);
        
    private:
        EventRouter* eventRouter_ = nullptr;
        std::shared_ptr<std::function<Event(uint16_t)>> eventHandler_;
    };
    
    /**
     * @brief Extended draw area builder for mouse events.
     */
    class DrawAreaBuilder {
    public:
        explicit DrawAreaBuilder(lv_obj_t* parent);
        
        DrawAreaBuilder& size(int width, int height);
        DrawAreaBuilder& position(int x, int y, lv_align_t align = LV_ALIGN_TOP_LEFT);
        
        /**
         * @brief Set up mouse event handling.
         * Automatically generates MouseDownEvent, MouseMoveEvent, MouseUpEvent
         */
        DrawAreaBuilder& onMouseEvents();
        
        /**
         * @brief Set custom mouse event handlers.
         */
        DrawAreaBuilder& onMouseDown(std::function<Event(int, int)> handler);
        DrawAreaBuilder& onMouseMove(std::function<Event(int, int)> handler);
        DrawAreaBuilder& onMouseUp(std::function<Event(int, int)> handler);
        
        DrawAreaBuilder& withEventRouter(EventRouter* router);
        
        Result<lv_obj_t*, std::string> build();
        lv_obj_t* buildOrLog();
        
    private:
        lv_obj_t* parent_;
        lv_obj_t* drawArea_;
        LVGLBuilder::Size size_{400, 400};
        LVGLBuilder::Position position_{0, 0};
        EventRouter* eventRouter_ = nullptr;
        
        std::shared_ptr<std::function<Event(int, int)>> mouseDownHandler_;
        std::shared_ptr<std::function<Event(int, int)>> mouseMoveHandler_;
        std::shared_ptr<std::function<Event(int, int)>> mouseUpHandler_;
        
        void setupMouseEvents();
        std::pair<int, int> getRelativeCoords(lv_obj_t* obj, lv_point_t* point);
    };
    
    // Factory method for draw area
    static DrawAreaBuilder drawArea(lv_obj_t* parent, EventRouter* router);
    
    /**
     * @brief Helper to create event callback data that persists properly.
     * LVGL callbacks need stable pointers, this ensures they stay valid.
     */
    template<typename T>
    static void* createCallbackData(std::shared_ptr<T> data) {
        // Store shared_ptr in a stable location
        auto* holder = new std::shared_ptr<T>(data);
        return holder;
    }
    
    /**
     * @brief Generic LVGL event callback that routes to our event system.
     */
    static void eventCallback(lv_event_t* e);
};

} // namespace DirtSim
