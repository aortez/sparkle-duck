#include "LVGLBuilder.h"
#include "spdlog/spdlog.h"
#include <cstdio>

// Result utilities.
template<typename T>
auto Ok(T&& value) {
    return Result<std::decay_t<T>, std::string>::okay(std::forward<T>(value));
}

template<typename E>
auto Error(const E& error) {
    return Result<lv_obj_t*, E>(error);
}

// ============================================================================
// SliderBuilder Implementation.
// ============================================================================

LVGLBuilder::SliderBuilder::SliderBuilder(lv_obj_t* parent)
    : parent_(parent),
      slider_(nullptr),
      label_(nullptr),
      value_label_(nullptr),
      size_(200, 10),
      position_(0, 0, LV_ALIGN_TOP_LEFT),
      min_value_(0),
      max_value_(100),
      initial_value_(50),
      callback_(nullptr),
      user_data_(nullptr),
      callback_data_factory_(nullptr),
      use_factory_(false),
      event_code_(LV_EVENT_ALL),
      label_position_(0, -25, LV_ALIGN_TOP_LEFT),
      has_label_(false),
      value_label_position_(110, -25, LV_ALIGN_TOP_LEFT),
      has_value_label_(false)
{
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::size(int width, int height) {
    size_ = Size(width, height);
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::size(const Size& sz) {
    size_ = sz;
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::position(int x, int y, lv_align_t align) {
    position_ = Position(x, y, align);
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::position(const Position& pos) {
    position_ = pos;
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::range(int min, int max) {
    if (min >= max) {
        spdlog::warn("SliderBuilder: Invalid range [{}, {}] - min must be less than max", min, max);
        return *this;
    }
    min_value_ = min;
    max_value_ = max;
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::value(int initial_value) {
    initial_value_ = initial_value;
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::label(const char* text, int offset_x, int offset_y) {
    if (!text) {
        spdlog::warn("SliderBuilder: null text provided for label");
        return *this;
    }
    label_text_ = text;
    label_position_ = Position(position_.x + offset_x, position_.y + offset_y, position_.align);
    has_label_ = true;
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::valueLabel(const char* format, int offset_x, int offset_y) {
    if (!format) {
        spdlog::warn("SliderBuilder: null format provided for value label");
        return *this;
    }
    value_format_ = format;
    value_label_position_ = Position(position_.x + offset_x, position_.y + offset_y, position_.align);
    has_value_label_ = true;
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::callback(lv_event_cb_t cb, void* user_data) {
    callback_ = cb;
    user_data_ = user_data;
    use_factory_ = false;
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::callback(lv_event_cb_t cb, std::function<void*(lv_obj_t*)> callback_data_factory) {
    callback_ = cb;
    callback_data_factory_ = callback_data_factory;
    use_factory_ = true;
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::events(lv_event_code_t event_code) {
    event_code_ = event_code;
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::valueTransform(std::function<double(int32_t)> transform) {
    value_transform_ = transform;
    return *this;
}

Result<lv_obj_t*, std::string> LVGLBuilder::SliderBuilder::build() {
    if (!parent_) {
        std::string error = "SliderBuilder: parent cannot be null";
        spdlog::error(error);
        return Result<lv_obj_t*, std::string>::error(error);
    }

    if (min_value_ >= max_value_) {
        std::string error = "SliderBuilder: invalid range [" + std::to_string(min_value_) + 
                           ", " + std::to_string(max_value_) + "] - min must be less than max";
        spdlog::error(error);
        return Result<lv_obj_t*, std::string>::error(error);
    }

    // Create slider.
    auto result = createSlider();
    if (result.isError()) {
        return result;
    }

    // Create optional labels.
    if (has_label_) {
        createLabel();
    }
    
    if (has_value_label_) {
        createValueLabel();
    }

    // Setup events.
    if (callback_) {
        setupEvents();
    }

    spdlog::debug("SliderBuilder: Successfully created slider at ({}, {}) with range [{}, {}]",
                 position_.x, position_.y, min_value_, max_value_);

    return Result<lv_obj_t*, std::string>::okay(slider_);
}

lv_obj_t* LVGLBuilder::SliderBuilder::buildOrLog() {
    auto result = build();
    if (result.isError()) {
        spdlog::error("SliderBuilder::buildOrLog failed: {}", result.error());
        return nullptr;
    }
    return result.value();
}

Result<lv_obj_t*, std::string> LVGLBuilder::SliderBuilder::createSlider() {
    slider_ = lv_slider_create(parent_);
    if (!slider_) {
        std::string error = "SliderBuilder: Failed to create slider object";
        spdlog::error(error);
        return Result<lv_obj_t*, std::string>::error(error);
    }

    // Set size.
    lv_obj_set_size(slider_, size_.width, size_.height);
    
    // Set position.
    lv_obj_align(slider_, position_.align, position_.x, position_.y);
    
    // Set range.
    lv_slider_set_range(slider_, min_value_, max_value_);
    
    // Set initial value (clamp to range).
    int clamped_value = std::max(min_value_, std::min(max_value_, initial_value_));
    if (clamped_value != initial_value_) {
        spdlog::warn("SliderBuilder: Initial value {} clamped to range [{}, {}], using {}",
                    initial_value_, min_value_, max_value_, clamped_value);
    }
    lv_slider_set_value(slider_, clamped_value, LV_ANIM_OFF);

    return Result<lv_obj_t*, std::string>::okay(slider_);
}

void LVGLBuilder::SliderBuilder::createLabel() {
    label_ = lv_label_create(parent_);
    if (!label_) {
        spdlog::warn("SliderBuilder: Failed to create label object");
        return;
    }
    
    lv_label_set_text(label_, label_text_.c_str());
    lv_obj_align(label_, label_position_.align, label_position_.x, label_position_.y);
}

void LVGLBuilder::SliderBuilder::createValueLabel() {
    value_label_ = lv_label_create(parent_);
    if (!value_label_) {
        spdlog::warn("SliderBuilder: Failed to create value label object");
        return;
    }
    
    // Set initial value text based on slider's current value.
    char buf[32];
    int32_t current_value = lv_slider_get_value(slider_);
    
    // Apply transform if provided, otherwise use raw value.
    double display_value;
    if (value_transform_) {
        display_value = value_transform_(current_value);
    } else {
        display_value = static_cast<double>(current_value);
    }
    
    snprintf(buf, sizeof(buf), value_format_.c_str(), display_value);
    lv_label_set_text(value_label_, buf);
    
    lv_obj_align(value_label_, value_label_position_.align, value_label_position_.x, value_label_position_.y);
}

void LVGLBuilder::SliderBuilder::setupEvents() {
    void* user_data = user_data_;
    
    // If using factory, create callback data with value label.
    if (use_factory_ && callback_data_factory_) {
        user_data = callback_data_factory_(value_label_);
    }
    
    // Add user's callback.
    if (callback_) {
        lv_obj_add_event_cb(slider_, callback_, event_code_, user_data);
    }
    
    // Add auto-update callback for value label if we have one.
    if (value_label_ && has_value_label_) {
        // Create persistent data for the value label callback.
        ValueLabelData* data = new ValueLabelData{
            value_label_,
            value_format_,
            value_transform_
        };
        
        // Add the value update callback with the persistent data.
        lv_obj_add_event_cb(slider_, valueUpdateCallback, LV_EVENT_VALUE_CHANGED, data);
        
        // Add a delete callback to clean up the allocated data.
        lv_obj_add_event_cb(slider_, sliderDeleteCallback, LV_EVENT_DELETE, data);
    }
}


void LVGLBuilder::SliderBuilder::valueUpdateCallback(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        ValueLabelData* data = static_cast<ValueLabelData*>(lv_event_get_user_data(e));
        if (data && data->value_label) {
            lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
            char buf[32];
            int32_t current_value = lv_slider_get_value(slider);
            
            // Apply transform if provided, otherwise use raw value.
            double display_value;
            if (data->transform) {
                display_value = data->transform(current_value);
            } else {
                display_value = static_cast<double>(current_value);
            }
            
            snprintf(buf, sizeof(buf), data->format.c_str(), display_value);
            lv_label_set_text(data->value_label, buf);
        }
    }
}

void LVGLBuilder::SliderBuilder::sliderDeleteCallback(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_DELETE) {
        ValueLabelData* data = static_cast<ValueLabelData*>(lv_event_get_user_data(e));
        delete data;
    }
}

// ============================================================================
// ButtonBuilder Implementation.
// ============================================================================

LVGLBuilder::ButtonBuilder::ButtonBuilder(lv_obj_t* parent)
    : parent_(parent),
      button_(nullptr),
      label_(nullptr),
      size_(100, 40),
      position_(0, 0, LV_ALIGN_TOP_LEFT),
      is_toggle_(false),
      is_checkable_(false),
      callback_(nullptr),
      user_data_(nullptr),
      event_code_(LV_EVENT_CLICKED)
{
}

LVGLBuilder::ButtonBuilder& LVGLBuilder::ButtonBuilder::size(int width, int height) {
    size_ = Size(width, height);
    return *this;
}

LVGLBuilder::ButtonBuilder& LVGLBuilder::ButtonBuilder::size(const Size& sz) {
    size_ = sz;
    return *this;
}

LVGLBuilder::ButtonBuilder& LVGLBuilder::ButtonBuilder::position(int x, int y, lv_align_t align) {
    position_ = Position(x, y, align);
    return *this;
}

LVGLBuilder::ButtonBuilder& LVGLBuilder::ButtonBuilder::position(const Position& pos) {
    position_ = pos;
    return *this;
}

LVGLBuilder::ButtonBuilder& LVGLBuilder::ButtonBuilder::text(const char* text) {
    if (!text) {
        spdlog::warn("ButtonBuilder: null text provided");
        return *this;
    }
    text_ = text;
    return *this;
}

LVGLBuilder::ButtonBuilder& LVGLBuilder::ButtonBuilder::toggle(bool enabled) {
    is_toggle_ = enabled;
    return *this;
}

LVGLBuilder::ButtonBuilder& LVGLBuilder::ButtonBuilder::checkable(bool enabled) {
    is_checkable_ = enabled;
    return *this;
}

LVGLBuilder::ButtonBuilder& LVGLBuilder::ButtonBuilder::callback(lv_event_cb_t cb, void* user_data) {
    callback_ = cb;
    user_data_ = user_data;
    return *this;
}

LVGLBuilder::ButtonBuilder& LVGLBuilder::ButtonBuilder::events(lv_event_code_t event_code) {
    event_code_ = event_code;
    return *this;
}

Result<lv_obj_t*, std::string> LVGLBuilder::ButtonBuilder::build() {
    if (!parent_) {
        std::string error = "ButtonBuilder: parent cannot be null";
        spdlog::error(error);
        return Result<lv_obj_t*, std::string>::error(error);
    }

    // Create button.
    auto result = createButton();
    if (result.isError()) {
        return result;
    }

    // Create label if text provided.
    if (!text_.empty()) {
        createLabel();
    }

    // Setup behavior (toggle, checkable).
    setupBehavior();

    // Setup events.
    if (callback_) {
        setupEvents();
    }

    spdlog::debug("ButtonBuilder: Successfully created button '{}' at ({}, {})",
                 text_, position_.x, position_.y);

    return Result<lv_obj_t*, std::string>::okay(button_);
}

lv_obj_t* LVGLBuilder::ButtonBuilder::buildOrLog() {
    auto result = build();
    if (result.isError()) {
        spdlog::error("ButtonBuilder::buildOrLog failed: {}", result.error());
        return nullptr;
    }
    return result.value();
}

Result<lv_obj_t*, std::string> LVGLBuilder::ButtonBuilder::createButton() {
    button_ = lv_btn_create(parent_);
    if (!button_) {
        std::string error = "ButtonBuilder: Failed to create button object";
        spdlog::error(error);
        return Result<lv_obj_t*, std::string>::error(error);
    }

    // Set size.
    lv_obj_set_size(button_, size_.width, size_.height);
    
    // Set position.
    lv_obj_align(button_, position_.align, position_.x, position_.y);

    return Result<lv_obj_t*, std::string>::okay(button_);
}

void LVGLBuilder::ButtonBuilder::createLabel() {
    label_ = lv_label_create(button_);
    if (!label_) {
        spdlog::warn("ButtonBuilder: Failed to create label object");
        return;
    }
    
    lv_label_set_text(label_, text_.c_str());
    lv_obj_center(label_);
}

void LVGLBuilder::ButtonBuilder::setupBehavior() {
    if (is_checkable_) {
        lv_obj_add_flag(button_, LV_OBJ_FLAG_CHECKABLE);
    }
    
    // Note: LVGL doesn't have a specific "toggle" flag - toggle behavior.
    // is typically implemented through checkable flag and event handling.
    if (is_toggle_) {
        lv_obj_add_flag(button_, LV_OBJ_FLAG_CHECKABLE);
    }
}

void LVGLBuilder::ButtonBuilder::setupEvents() {
    lv_obj_add_event_cb(button_, callback_, event_code_, user_data_);
}

// ============================================================================
// LabelBuilder Implementation.
// ============================================================================

LVGLBuilder::LabelBuilder::LabelBuilder(lv_obj_t* parent)
    : parent_(parent),
      position_(0, 0, LV_ALIGN_TOP_LEFT)
{
}

LVGLBuilder::LabelBuilder& LVGLBuilder::LabelBuilder::text(const char* text) {
    if (!text) {
        spdlog::warn("LabelBuilder: null text provided");
        return *this;
    }
    text_ = text;
    return *this;
}

LVGLBuilder::LabelBuilder& LVGLBuilder::LabelBuilder::position(int x, int y, lv_align_t align) {
    position_ = Position(x, y, align);
    return *this;
}

LVGLBuilder::LabelBuilder& LVGLBuilder::LabelBuilder::position(const Position& pos) {
    position_ = pos;
    return *this;
}

Result<lv_obj_t*, std::string> LVGLBuilder::LabelBuilder::build() {
    if (!parent_) {
        std::string error = "LabelBuilder: parent cannot be null";
        spdlog::error(error);
        return Result<lv_obj_t*, std::string>::error(error);
    }

    lv_obj_t* label = lv_label_create(parent_);
    if (!label) {
        std::string error = "LabelBuilder: Failed to create label object";
        spdlog::error(error);
        return Result<lv_obj_t*, std::string>::error(error);
    }

    if (!text_.empty()) {
        lv_label_set_text(label, text_.c_str());
    }
    
    lv_obj_align(label, position_.align, position_.x, position_.y);

    spdlog::debug("LabelBuilder: Successfully created label '{}' at ({}, {})",
                 text_, position_.x, position_.y);

    return Result<lv_obj_t*, std::string>::okay(label);
}

// ============================================================================
// DropdownBuilder Implementation.
// ============================================================================

LVGLBuilder::DropdownBuilder::DropdownBuilder(lv_obj_t* parent)
    : parent_(parent)
    , position_(0, 0, LV_ALIGN_TOP_LEFT)
    , size_(150, 40) {
}

LVGLBuilder::DropdownBuilder& LVGLBuilder::DropdownBuilder::options(const char* options) {
    options_ = options ? options : "";
    return *this;
}

LVGLBuilder::DropdownBuilder& LVGLBuilder::DropdownBuilder::selected(uint16_t index) {
    selectedIndex_ = index;
    return *this;
}

LVGLBuilder::DropdownBuilder& LVGLBuilder::DropdownBuilder::position(int x, int y, lv_align_t align) {
    position_ = Position(x, y, align);
    return *this;
}

LVGLBuilder::DropdownBuilder& LVGLBuilder::DropdownBuilder::position(const Position& pos) {
    position_ = pos;
    return *this;
}

LVGLBuilder::DropdownBuilder& LVGLBuilder::DropdownBuilder::size(int width, int height) {
    size_ = Size(width, height);
    return *this;
}

LVGLBuilder::DropdownBuilder& LVGLBuilder::DropdownBuilder::size(const Size& s) {
    size_ = s;
    return *this;
}

Result<lv_obj_t*, std::string> LVGLBuilder::DropdownBuilder::build() {
    if (!parent_) {
        return Error<std::string>("DropdownBuilder: parent is null");
    }
    
    lv_obj_t* dropdown = lv_dropdown_create(parent_);
    if (!dropdown) {
        return Error<std::string>("DropdownBuilder: failed to create dropdown");
    }
    
    // Set options.
    if (!options_.empty()) {
        lv_dropdown_set_options(dropdown, options_.c_str());
    }
    
    // Set selected index.
    lv_dropdown_set_selected(dropdown, selectedIndex_);
    
    // Set size and position.
    lv_obj_set_size(dropdown, size_.width, size_.height);
    lv_obj_align(dropdown, position_.align, position_.x, position_.y);
    
    return Ok(dropdown);
}

lv_obj_t* LVGLBuilder::DropdownBuilder::buildOrLog() {
    auto result = build();
    if (result.isError()) {
        spdlog::error("DropdownBuilder::buildOrLog failed: {}", result.error());
        return nullptr;
    }
    return result.value();
}

// ============================================================================
// Static Factory Methods.
// ============================================================================

LVGLBuilder::SliderBuilder LVGLBuilder::slider(lv_obj_t* parent) {
    return SliderBuilder(parent);
}

LVGLBuilder::ButtonBuilder LVGLBuilder::button(lv_obj_t* parent) {
    return ButtonBuilder(parent);
}

LVGLBuilder::DropdownBuilder LVGLBuilder::dropdown(lv_obj_t* parent) {
    return DropdownBuilder(parent);
}
