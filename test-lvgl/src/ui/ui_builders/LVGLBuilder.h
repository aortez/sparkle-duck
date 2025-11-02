#pragma once

#include "../../core/Result.h"
#include "lvgl/lvgl.h"
#include <cmath>
#include <functional>
#include <string>

/**
 * LVGLBuilder provides a fluent interface for creating LVGL UI elements
 * with reduced boilerplate and consistent patterns.
 * 
 * Example usage:
 *   auto slider = LVGLBuilder::slider(parent)
 *       .position(100, 50)
 *       .size(200, 10)
 *       .range(0, 100)
 *       .value(50)
 *       .label("Volume")
 *       .valueLabel("%.0f")
 *       .callback(volumeCallback, userData)
 *       .build();
 */
class LVGLBuilder {
public:
    // Forward declaration for position specification.
    struct Position {
        int x, y;
        lv_align_t align;
        
        Position(int x, int y, lv_align_t align = LV_ALIGN_TOP_LEFT) 
            : x(x), y(y), align(align) {}
    };

    // Forward declaration for size specification.
    struct Size {
        int width, height;
        
        Size(int width, int height) : width(width), height(height) {}
    };

    /**
     * SliderBuilder - Fluent interface for creating sliders with labels and callbacks.
     */
    class SliderBuilder {
    public:
        explicit SliderBuilder(lv_obj_t* parent);
        
        // Core slider configuration.
        SliderBuilder& size(int width, int height = 10);
        SliderBuilder& size(const Size& sz);
        SliderBuilder& position(int x, int y, lv_align_t align = LV_ALIGN_TOP_LEFT);
        SliderBuilder& position(const Position& pos);
        SliderBuilder& range(int min, int max);
        SliderBuilder& value(int initial_value);
        
        // Label configuration.
        SliderBuilder& label(const char* text, int offset_x = 0, int offset_y = -20);
        SliderBuilder& valueLabel(const char* format = "%.1f", int offset_x = 110, int offset_y = -20);
        
        // Value transformation for display.
        SliderBuilder& valueTransform(std::function<double(int32_t)> transform);
        
        // Event handling.
        SliderBuilder& callback(lv_event_cb_t cb, void* user_data = nullptr);
        SliderBuilder& callback(lv_event_cb_t cb, std::function<void*(lv_obj_t*)> callback_data_factory);
        SliderBuilder& events(lv_event_code_t event_code = LV_EVENT_ALL);
        
        // Build the final slider (returns the slider object, not the container).
        Result<lv_obj_t*, std::string> build();
        
        // Build with automatic error logging (returns slider or nullptr).
        lv_obj_t* buildOrLog();
        
        // Access to created objects after build() for advanced use cases.
        lv_obj_t* getSlider() const { return slider_; }
        lv_obj_t* getLabel() const { return label_; }
        lv_obj_t* getValueLabel() const { return value_label_; }
        
    private:
        lv_obj_t* parent_;
        lv_obj_t* slider_;
        lv_obj_t* label_;
        lv_obj_t* value_label_;
        
        // Configuration storage.
        Size size_;
        Position position_;
        int min_value_, max_value_;
        int initial_value_;
        lv_event_cb_t callback_;
        void* user_data_;
        std::function<void*(lv_obj_t*)> callback_data_factory_;
        bool use_factory_;
        lv_event_code_t event_code_;
        
        // Label configuration.
        std::string label_text_;
        Position label_position_;
        bool has_label_;
        
        std::string value_format_;
        Position value_label_position_;
        bool has_value_label_;
        std::function<double(int32_t)> value_transform_;
        
        // Structure to hold value label update data.
        struct ValueLabelData {
            lv_obj_t* value_label;
            std::string format;
            std::function<double(int32_t)> transform;
        };
        
        Result<lv_obj_t*, std::string> createSlider();
        void createLabel();
        void createValueLabel();
        void setupEvents();
        
        // Static callbacks.
        static void valueUpdateCallback(lv_event_t* e);
        static void sliderDeleteCallback(lv_event_t* e);
    };

    /**
     * ButtonBuilder - Fluent interface for creating buttons with text and callbacks.
     */
    class ButtonBuilder {
    public:
        explicit ButtonBuilder(lv_obj_t* parent);
        
        // Core button configuration.
        ButtonBuilder& size(int width, int height);
        ButtonBuilder& size(const Size& sz);
        ButtonBuilder& position(int x, int y, lv_align_t align = LV_ALIGN_TOP_LEFT);
        ButtonBuilder& position(const Position& pos);
        ButtonBuilder& text(const char* text);
        
        // Button behavior.
        ButtonBuilder& toggle(bool enabled = true);
        ButtonBuilder& checkable(bool enabled = true);
        
        // Event handling.
        ButtonBuilder& callback(lv_event_cb_t cb, void* user_data = nullptr);
        ButtonBuilder& events(lv_event_code_t event_code = LV_EVENT_CLICKED);
        
        // Build the final button.
        Result<lv_obj_t*, std::string> build();
        
        // Build with automatic error logging (returns button or nullptr).
        lv_obj_t* buildOrLog();
        
        // Access to created objects.
        lv_obj_t* getButton() const { return button_; }
        lv_obj_t* getLabel() const { return label_; }
        
    private:
        lv_obj_t* parent_;
        lv_obj_t* button_;
        lv_obj_t* label_;
        
        // Configuration storage.
        Size size_;
        Position position_;
        std::string text_;
        bool is_toggle_;
        bool is_checkable_;
        lv_event_cb_t callback_;
        void* user_data_;
        lv_event_code_t event_code_;
        
        Result<lv_obj_t*, std::string> createButton();
        void createLabel();
        void setupBehavior();
        void setupEvents();
    };

    /**
     * LabelBuilder - Simple interface for creating labels.
     */
    class LabelBuilder {
    public:
        explicit LabelBuilder(lv_obj_t* parent);
        
        LabelBuilder& text(const char* text);
        LabelBuilder& position(int x, int y, lv_align_t align = LV_ALIGN_TOP_LEFT);
        LabelBuilder& position(const Position& pos);
        
        Result<lv_obj_t*, std::string> build();
        
        // Build with automatic error logging (returns label or nullptr).
        lv_obj_t* buildOrLog();
        
    private:
        lv_obj_t* parent_;
        std::string text_;
        Position position_;
    };
    
    /**
     * DropdownBuilder - Interface for creating dropdown widgets.
     */
    class DropdownBuilder {
    public:
        explicit DropdownBuilder(lv_obj_t* parent);
        
        DropdownBuilder& options(const char* options);
        DropdownBuilder& selected(uint16_t index);
        DropdownBuilder& position(int x, int y, lv_align_t align = LV_ALIGN_TOP_LEFT);
        DropdownBuilder& position(const Position& pos);
        DropdownBuilder& size(int width, int height);
        DropdownBuilder& size(const Size& size);
        
        Result<lv_obj_t*, std::string> build();
        
        // Build with automatic error logging (returns dropdown or nullptr).
        lv_obj_t* buildOrLog();
        
    private:
        lv_obj_t* parent_;
        std::string options_;
        uint16_t selectedIndex_ = 0;
        Position position_;
        Size size_;
    };

    // Static factory methods for fluent interface.
    static SliderBuilder slider(lv_obj_t* parent);
    static ButtonBuilder button(lv_obj_t* parent);
    static LabelBuilder label(lv_obj_t* parent);
    static DropdownBuilder dropdown(lv_obj_t* parent);
    
    // Common value transform functions for sliders.
    struct Transforms {
        // Linear scaling: value * scale
        static std::function<double(int32_t)> Linear(double scale) {
            return [scale](int32_t value) { return value * scale; };
        }
        
        // Exponential scaling: base^(value * scale + offset)
        static std::function<double(int32_t)> Exponential(double base, double scale, double offset = 0) {
            return [base, scale, offset](int32_t value) { 
                return std::pow(base, value * scale + offset); 
            };
        }
        
        // Percentage: value as-is (for 0-100 ranges)
        static std::function<double(int32_t)> Percentage() {
            return [](int32_t value) { return static_cast<double>(value); };
        }
        
        // Logarithmic: log(1 + value * scale)
        static std::function<double(int32_t)> Logarithmic(double scale = 1.0) {
            return [scale](int32_t value) { return std::log1p(value * scale); };
        }
    };
    
    // Utility methods for common positioning patterns.
    static Position topLeft(int x, int y) { return Position(x, y, LV_ALIGN_TOP_LEFT); }
    static Position topRight(int x, int y) { return Position(x, y, LV_ALIGN_TOP_RIGHT); }
    static Position center(int x = 0, int y = 0) { return Position(x, y, LV_ALIGN_CENTER); }
    
    // Common size presets for consistency.
    static Size sliderSize(int width = 200) { return Size(width, 10); }
    static Size buttonSize(int width = 100, int height = 40) { return Size(width, height); }
    static Size smallButton(int width = 80, int height = 30) { return Size(width, height); }
};