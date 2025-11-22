#include "LVGLBuilder.h"
#include "spdlog/spdlog.h"
#include <cstdio>

// Result utilities.
template <typename T>
auto Ok(T&& value)
{
    return Result<std::decay_t<T>, std::string>::okay(std::forward<T>(value));
}

template <typename E>
auto Error(const E& error)
{
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
{}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::size(int width, int height)
{
    size_ = Size(width, height);
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::size(const Size& sz)
{
    size_ = sz;
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::position(int x, int y, lv_align_t align)
{
    position_ = Position(x, y, align);
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::position(const Position& pos)
{
    position_ = pos;
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::range(int min, int max)
{
    if (min >= max) {
        spdlog::warn("SliderBuilder: Invalid range [{}, {}] - min must be less than max", min, max);
        return *this;
    }
    min_value_ = min;
    max_value_ = max;
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::value(int initial_value)
{
    initial_value_ = initial_value;
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::label(
    const char* text, int offset_x, int offset_y)
{
    if (!text) {
        spdlog::warn("SliderBuilder: null text provided for label");
        return *this;
    }
    label_text_ = text;
    label_position_ = Position(position_.x + offset_x, position_.y + offset_y, position_.align);
    has_label_ = true;
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::valueLabel(
    const char* format, int offset_x, int offset_y)
{
    if (!format) {
        spdlog::warn("SliderBuilder: null format provided for value label");
        return *this;
    }
    value_format_ = format;
    value_label_position_ =
        Position(position_.x + offset_x, position_.y + offset_y, position_.align);
    has_value_label_ = true;
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::callback(lv_event_cb_t cb, void* user_data)
{
    callback_ = cb;
    user_data_ = user_data;
    use_factory_ = false;
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::callback(
    lv_event_cb_t cb, std::function<void*(lv_obj_t*)> callback_data_factory)
{
    callback_ = cb;
    callback_data_factory_ = callback_data_factory;
    use_factory_ = true;
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::events(lv_event_code_t event_code)
{
    event_code_ = event_code;
    return *this;
}

LVGLBuilder::SliderBuilder& LVGLBuilder::SliderBuilder::valueTransform(
    std::function<double(int32_t)> transform)
{
    value_transform_ = transform;
    return *this;
}

Result<lv_obj_t*, std::string> LVGLBuilder::SliderBuilder::build()
{
    if (!parent_) {
        std::string error = "SliderBuilder: parent cannot be null";
        spdlog::error(error);
        return Result<lv_obj_t*, std::string>::error(error);
    }

    if (min_value_ >= max_value_) {
        std::string error = "SliderBuilder: invalid range [" + std::to_string(min_value_) + ", "
            + std::to_string(max_value_) + "] - min must be less than max";
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

    spdlog::debug(
        "SliderBuilder: Successfully created slider at ({}, {}) with range [{}, {}]",
        position_.x,
        position_.y,
        min_value_,
        max_value_);

    return Result<lv_obj_t*, std::string>::okay(slider_);
}

lv_obj_t* LVGLBuilder::SliderBuilder::buildOrLog()
{
    auto result = build();
    if (result.isError()) {
        spdlog::error("SliderBuilder::buildOrLog failed: {}", result.error());
        return nullptr;
    }
    return result.value();
}

Result<lv_obj_t*, std::string> LVGLBuilder::SliderBuilder::createSlider()
{
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
        spdlog::warn(
            "SliderBuilder: Initial value {} clamped to range [{}, {}], using {}",
            initial_value_,
            min_value_,
            max_value_,
            clamped_value);
    }
    lv_slider_set_value(slider_, clamped_value, LV_ANIM_OFF);

    return Result<lv_obj_t*, std::string>::okay(slider_);
}

void LVGLBuilder::SliderBuilder::createLabel()
{
    label_ = lv_label_create(parent_);
    if (!label_) {
        spdlog::warn("SliderBuilder: Failed to create label object");
        return;
    }

    lv_label_set_text(label_, label_text_.c_str());
    lv_obj_set_style_text_color(label_, lv_color_hex(0xFFFFFF), 0); // White text.
    lv_obj_align(label_, label_position_.align, label_position_.x, label_position_.y);
}

void LVGLBuilder::SliderBuilder::createValueLabel()
{
    value_label_ = lv_label_create(parent_);
    if (!value_label_) {
        spdlog::warn("SliderBuilder: Failed to create value label object");
        return;
    }

    lv_obj_set_style_text_color(value_label_, lv_color_hex(0xFFFFFF), 0); // White text.

    // Set initial value text based on slider's current value.
    char buf[32];
    int32_t current_value = lv_slider_get_value(slider_);

    // Apply transform if provided, otherwise use raw value.
    double display_value;
    if (value_transform_) {
        display_value = value_transform_(current_value);
    }
    else {
        display_value = static_cast<double>(current_value);
    }

    snprintf(buf, sizeof(buf), value_format_.c_str(), display_value);
    lv_label_set_text(value_label_, buf);

    lv_obj_align(
        value_label_,
        value_label_position_.align,
        value_label_position_.x,
        value_label_position_.y);
}

void LVGLBuilder::SliderBuilder::setupEvents()
{
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
        ValueLabelData* data = new ValueLabelData{ value_label_, value_format_, value_transform_ };

        // Add the value update callback with the persistent data.
        lv_obj_add_event_cb(slider_, valueUpdateCallback, LV_EVENT_VALUE_CHANGED, data);

        // Add a delete callback to clean up the allocated data.
        lv_obj_add_event_cb(slider_, sliderDeleteCallback, LV_EVENT_DELETE, data);
    }
}

void LVGLBuilder::SliderBuilder::valueUpdateCallback(lv_event_t* e)
{
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
            }
            else {
                display_value = static_cast<double>(current_value);
            }

            snprintf(buf, sizeof(buf), data->format.c_str(), display_value);
            lv_label_set_text(data->value_label, buf);
        }
    }
}

void LVGLBuilder::SliderBuilder::sliderDeleteCallback(lv_event_t* e)
{
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
{}

LVGLBuilder::ButtonBuilder& LVGLBuilder::ButtonBuilder::size(int width, int height)
{
    size_ = Size(width, height);
    return *this;
}

LVGLBuilder::ButtonBuilder& LVGLBuilder::ButtonBuilder::size(const Size& sz)
{
    size_ = sz;
    return *this;
}

LVGLBuilder::ButtonBuilder& LVGLBuilder::ButtonBuilder::position(int x, int y, lv_align_t align)
{
    position_ = Position(x, y, align);
    return *this;
}

LVGLBuilder::ButtonBuilder& LVGLBuilder::ButtonBuilder::position(const Position& pos)
{
    position_ = pos;
    return *this;
}

LVGLBuilder::ButtonBuilder& LVGLBuilder::ButtonBuilder::text(const char* text)
{
    if (!text) {
        spdlog::warn("ButtonBuilder: null text provided");
        return *this;
    }
    text_ = text;
    return *this;
}

LVGLBuilder::ButtonBuilder& LVGLBuilder::ButtonBuilder::toggle(bool enabled)
{
    is_toggle_ = enabled;
    return *this;
}

LVGLBuilder::ButtonBuilder& LVGLBuilder::ButtonBuilder::checkable(bool enabled)
{
    is_checkable_ = enabled;
    return *this;
}

LVGLBuilder::ButtonBuilder& LVGLBuilder::ButtonBuilder::callback(lv_event_cb_t cb, void* user_data)
{
    callback_ = cb;
    user_data_ = user_data;
    return *this;
}

LVGLBuilder::ButtonBuilder& LVGLBuilder::ButtonBuilder::events(lv_event_code_t event_code)
{
    event_code_ = event_code;
    return *this;
}

Result<lv_obj_t*, std::string> LVGLBuilder::ButtonBuilder::build()
{
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

    spdlog::debug(
        "ButtonBuilder: Successfully created button '{}' at ({}, {})",
        text_,
        position_.x,
        position_.y);

    return Result<lv_obj_t*, std::string>::okay(button_);
}

lv_obj_t* LVGLBuilder::ButtonBuilder::buildOrLog()
{
    auto result = build();
    if (result.isError()) {
        spdlog::error("ButtonBuilder::buildOrLog failed: {}", result.error());
        return nullptr;
    }
    return result.value();
}

Result<lv_obj_t*, std::string> LVGLBuilder::ButtonBuilder::createButton()
{
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

void LVGLBuilder::ButtonBuilder::createLabel()
{
    label_ = lv_label_create(button_);
    if (!label_) {
        spdlog::warn("ButtonBuilder: Failed to create label object");
        return;
    }

    lv_label_set_text(label_, text_.c_str());
    lv_obj_center(label_);
}

void LVGLBuilder::ButtonBuilder::setupBehavior()
{
    if (is_checkable_) {
        lv_obj_add_flag(button_, LV_OBJ_FLAG_CHECKABLE);
    }

    // Note: LVGL doesn't have a specific "toggle" flag - toggle behavior.
    // is typically implemented through checkable flag and event handling.
    if (is_toggle_) {
        lv_obj_add_flag(button_, LV_OBJ_FLAG_CHECKABLE);
    }
}

void LVGLBuilder::ButtonBuilder::setupEvents()
{
    // Set user_data on the button object itself so event handlers can retrieve it.
    if (user_data_) {
        lv_obj_set_user_data(button_, user_data_);
    }
    lv_obj_add_event_cb(button_, callback_, event_code_, user_data_);
}

// ============================================================================
// LabelBuilder Implementation.
// ============================================================================

LVGLBuilder::LabelBuilder::LabelBuilder(lv_obj_t* parent)
    : parent_(parent), position_(0, 0, LV_ALIGN_TOP_LEFT)
{}

LVGLBuilder::LabelBuilder& LVGLBuilder::LabelBuilder::text(const char* text)
{
    if (!text) {
        spdlog::warn("LabelBuilder: null text provided");
        return *this;
    }
    text_ = text;
    return *this;
}

LVGLBuilder::LabelBuilder& LVGLBuilder::LabelBuilder::position(int x, int y, lv_align_t align)
{
    position_ = Position(x, y, align);
    return *this;
}

LVGLBuilder::LabelBuilder& LVGLBuilder::LabelBuilder::position(const Position& pos)
{
    position_ = pos;
    return *this;
}

Result<lv_obj_t*, std::string> LVGLBuilder::LabelBuilder::build()
{
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

    spdlog::debug(
        "LabelBuilder: Successfully created label '{}' at ({}, {})",
        text_,
        position_.x,
        position_.y);

    return Result<lv_obj_t*, std::string>::okay(label);
}

// ============================================================================
// DropdownBuilder Implementation.
// ============================================================================

LVGLBuilder::DropdownBuilder::DropdownBuilder(lv_obj_t* parent)
    : parent_(parent), position_(0, 0, LV_ALIGN_TOP_LEFT), size_(150, 40)
{}

LVGLBuilder::DropdownBuilder& LVGLBuilder::DropdownBuilder::options(const char* options)
{
    options_ = options ? options : "";
    return *this;
}

LVGLBuilder::DropdownBuilder& LVGLBuilder::DropdownBuilder::selected(uint16_t index)
{
    selectedIndex_ = index;
    return *this;
}

LVGLBuilder::DropdownBuilder& LVGLBuilder::DropdownBuilder::position(int x, int y, lv_align_t align)
{
    position_ = Position(x, y, align);
    return *this;
}

LVGLBuilder::DropdownBuilder& LVGLBuilder::DropdownBuilder::position(const Position& pos)
{
    position_ = pos;
    return *this;
}

LVGLBuilder::DropdownBuilder& LVGLBuilder::DropdownBuilder::size(int width, int height)
{
    size_ = Size(width, height);
    return *this;
}

LVGLBuilder::DropdownBuilder& LVGLBuilder::DropdownBuilder::size(const Size& s)
{
    size_ = s;
    return *this;
}

Result<lv_obj_t*, std::string> LVGLBuilder::DropdownBuilder::build()
{
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

lv_obj_t* LVGLBuilder::DropdownBuilder::buildOrLog()
{
    auto result = build();
    if (result.isError()) {
        spdlog::error("DropdownBuilder::buildOrLog failed: {}", result.error());
        return nullptr;
    }
    return result.value();
}

// ============================================================================
// LabeledSwitchBuilder Implementation.
// ============================================================================

LVGLBuilder::LabeledSwitchBuilder::LabeledSwitchBuilder(lv_obj_t* parent)
    : parent_(parent), container_(nullptr), switch_(nullptr), label_(nullptr)
{}

LVGLBuilder::LabeledSwitchBuilder& LVGLBuilder::LabeledSwitchBuilder::label(const char* text)
{
    label_text_ = text;
    return *this;
}

LVGLBuilder::LabeledSwitchBuilder& LVGLBuilder::LabeledSwitchBuilder::initialState(bool checked)
{
    initial_checked_ = checked;
    return *this;
}

LVGLBuilder::LabeledSwitchBuilder& LVGLBuilder::LabeledSwitchBuilder::callback(
    lv_event_cb_t cb, void* user_data)
{
    callback_ = cb;
    user_data_ = user_data;
    return *this;
}

Result<lv_obj_t*, std::string> LVGLBuilder::LabeledSwitchBuilder::build()
{
    return createLabeledSwitch();
}

lv_obj_t* LVGLBuilder::LabeledSwitchBuilder::buildOrLog()
{
    auto result = build();
    if (result.isError()) {
        spdlog::error("LabeledSwitchBuilder::buildOrLog failed: {}", result.error());
        return nullptr;
    }
    return result.value();
}

// Helper callback for container click to toggle switch.
static void labeledSwitchContainerClicked(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    lv_obj_t* container = static_cast<lv_obj_t*>(lv_event_get_target(e));
    lv_obj_t* switch_obj = static_cast<lv_obj_t*>(lv_obj_get_user_data(container));

    if (switch_obj) {
        // Toggle switch state.
        if (lv_obj_has_state(switch_obj, LV_STATE_CHECKED)) {
            lv_obj_clear_state(switch_obj, LV_STATE_CHECKED);
        }
        else {
            lv_obj_add_state(switch_obj, LV_STATE_CHECKED);
        }

        // Send VALUE_CHANGED event to trigger the callback.
        lv_obj_send_event(switch_obj, LV_EVENT_VALUE_CHANGED, nullptr);
    }
}

Result<lv_obj_t*, std::string> LVGLBuilder::LabeledSwitchBuilder::createLabeledSwitch()
{
    // Create horizontal container for switch + label.
    container_ = lv_obj_create(parent_);
    if (!container_) {
        return Result<lv_obj_t*, std::string>::error("Failed to create container");
    }

    lv_obj_set_size(container_, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        container_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(container_, 5, 0);    // Padding for better click target.
    lv_obj_set_style_pad_column(container_, 8, 0); // Gap between switch and label.

    lv_obj_set_style_bg_color(container_, lv_color_hex(0x0000FF), 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);

    // Rounded corners.
    lv_obj_set_style_radius(container_, 5, 0);

    // Create switch.
    switch_ = lv_switch_create(container_);
    if (!switch_) {
        return Result<lv_obj_t*, std::string>::error("Failed to create switch");
    }

    // Set initial state.
    if (initial_checked_) {
        lv_obj_add_state(switch_, LV_STATE_CHECKED);
    }

    // Set up callback.
    if (callback_) {
        lv_obj_add_event_cb(switch_, callback_, LV_EVENT_VALUE_CHANGED, user_data_);
    }

    // Create label.
    if (!label_text_.empty()) {
        label_ = lv_label_create(container_);
        if (label_) {
            lv_label_set_text(label_, label_text_.c_str());
            lv_obj_set_style_text_color(label_, lv_color_hex(0xFFFFFF), 0); // White text.
        }
    }

    // Store switch pointer in container's user data for click handler.
    lv_obj_set_user_data(container_, switch_);

    // Add click handler to container to toggle switch.
    lv_obj_add_event_cb(container_, labeledSwitchContainerClicked, LV_EVENT_CLICKED, nullptr);

    // Make container clickable.
    lv_obj_add_flag(container_, LV_OBJ_FLAG_CLICKABLE);

    return Result<lv_obj_t*, std::string>::okay(switch_);
}

// ============================================================================
// ToggleSliderBuilder Implementation.
// ============================================================================

// State structure for toggle slider callbacks.
struct ToggleSliderState {
    lv_obj_t* slider;
    lv_obj_t* valueLabel;
    lv_obj_t* switch_obj;
    double valueScale;
    std::string valueFormat;
    int savedValue;
    int defaultValue;
    lv_event_cb_t sliderCallback;
    void* sliderUserData;
    lv_event_cb_t toggleCallback;
    void* toggleUserData;
};

static void toggleSliderSwitchCallback(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;

    ToggleSliderState* state = static_cast<ToggleSliderState*>(lv_event_get_user_data(e));
    if (!state) return;

    bool isEnabled = lv_obj_has_state(state->switch_obj, LV_STATE_CHECKED);

    if (isEnabled) {
        // Toggle ON: Restore saved value (or use default).
        int valueToRestore = (state->savedValue > 0) ? state->savedValue : state->defaultValue;
        lv_slider_set_value(state->slider, valueToRestore, LV_ANIM_OFF);

        // Restore blue color.
        lv_obj_set_style_bg_color(
            state->slider, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(state->slider, lv_palette_main(LV_PALETTE_BLUE), LV_PART_KNOB);

        // Update value label.
        double scaledValue = valueToRestore * state->valueScale;
        char buf[32];
        snprintf(buf, sizeof(buf), state->valueFormat.c_str(), scaledValue);
        lv_label_set_text(state->valueLabel, buf);
    }
    else {
        // Toggle OFF: Save current value, set to 0, gray out slider.
        // Note: Slider stays interactive for auto-enable feature.
        int currentValue = lv_slider_get_value(state->slider);
        if (currentValue > 0) {
            state->savedValue = currentValue;
        }

        lv_slider_set_value(state->slider, 0, LV_ANIM_OFF);

        // Grey color when disabled (visual feedback only, still interactive).
        lv_obj_set_style_bg_color(state->slider, lv_color_hex(0x808080), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(state->slider, lv_color_hex(0x808080), LV_PART_KNOB);

        // Update value label to 0.
        char buf[32];
        snprintf(buf, sizeof(buf), state->valueFormat.c_str(), 0.0);
        lv_label_set_text(state->valueLabel, buf);
    }

    // Call user callback if provided.
    if (state->toggleCallback) {
        state->toggleCallback(e);
    }
}

static void toggleSliderValueCallback(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;

    ToggleSliderState* state = static_cast<ToggleSliderState*>(lv_event_get_user_data(e));
    if (!state) return;

    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int value = lv_slider_get_value(slider);
    double scaledValue = value * state->valueScale;

    // Update value label.
    char buf[32];
    snprintf(buf, sizeof(buf), state->valueFormat.c_str(), scaledValue);
    lv_label_set_text(state->valueLabel, buf);

    // Call user callback if provided.
    if (state->sliderCallback) {
        state->sliderCallback(e);
    }
}

static void toggleSliderAutoEnableCallback(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_PRESSED) return;

    ToggleSliderState* state = static_cast<ToggleSliderState*>(lv_event_get_user_data(e));
    if (!state) return;

    // Check if toggle is currently disabled.
    bool isEnabled = lv_obj_has_state(state->switch_obj, LV_STATE_CHECKED);
    if (!isEnabled) {
        // Auto-enable the toggle when user grabs disabled slider.
        lv_obj_add_state(state->switch_obj, LV_STATE_CHECKED);

        // Trigger the switch callback to restore value, enable slider, update colors, etc.
        lv_obj_send_event(state->switch_obj, LV_EVENT_VALUE_CHANGED, nullptr);
    }
}

static void toggleSliderDeleteCallback(lv_event_t* e)
{
    ToggleSliderState* state = static_cast<ToggleSliderState*>(lv_event_get_user_data(e));
    delete state;
}

LVGLBuilder::ToggleSliderBuilder::ToggleSliderBuilder(lv_obj_t* parent)
    : parent_(parent),
      container_(nullptr),
      switch_(nullptr),
      slider_(nullptr),
      label_(nullptr),
      valueLabel_(nullptr)
{}

LVGLBuilder::ToggleSliderBuilder& LVGLBuilder::ToggleSliderBuilder::label(const char* text)
{
    label_text_ = text;
    return *this;
}

LVGLBuilder::ToggleSliderBuilder& LVGLBuilder::ToggleSliderBuilder::sliderWidth(int width)
{
    slider_width_ = width;
    return *this;
}

LVGLBuilder::ToggleSliderBuilder& LVGLBuilder::ToggleSliderBuilder::range(int min, int max)
{
    range_min_ = min;
    range_max_ = max;
    return *this;
}

LVGLBuilder::ToggleSliderBuilder& LVGLBuilder::ToggleSliderBuilder::value(int initialValue)
{
    initial_value_ = initialValue;
    return *this;
}

LVGLBuilder::ToggleSliderBuilder& LVGLBuilder::ToggleSliderBuilder::defaultValue(int defValue)
{
    default_value_ = defValue;
    return *this;
}

LVGLBuilder::ToggleSliderBuilder& LVGLBuilder::ToggleSliderBuilder::valueScale(double scale)
{
    value_scale_ = scale;
    return *this;
}

LVGLBuilder::ToggleSliderBuilder& LVGLBuilder::ToggleSliderBuilder::valueFormat(const char* format)
{
    value_format_ = format;
    return *this;
}

LVGLBuilder::ToggleSliderBuilder& LVGLBuilder::ToggleSliderBuilder::initiallyEnabled(bool enabled)
{
    initially_enabled_ = enabled;
    return *this;
}

LVGLBuilder::ToggleSliderBuilder& LVGLBuilder::ToggleSliderBuilder::onToggle(
    lv_event_cb_t cb, void* user_data)
{
    toggle_callback_ = cb;
    toggle_user_data_ = user_data;
    return *this;
}

LVGLBuilder::ToggleSliderBuilder& LVGLBuilder::ToggleSliderBuilder::onSliderChange(
    lv_event_cb_t cb, void* user_data)
{
    slider_callback_ = cb;
    slider_user_data_ = user_data;
    return *this;
}

Result<lv_obj_t*, std::string> LVGLBuilder::ToggleSliderBuilder::createToggleSlider()
{
    // Create container for the whole control group.
    container_ = lv_obj_create(parent_);
    lv_obj_set_size(container_, LV_PCT(90), 60);
    lv_obj_set_style_pad_all(container_, 8, 0);
    lv_obj_set_style_border_width(container_, 1, 0);
    lv_obj_set_style_border_color(container_, lv_color_hex(0x404040), 0);
    lv_obj_set_style_radius(container_, 5, 0);
    lv_obj_clear_flag(container_, LV_OBJ_FLAG_SCROLLABLE);

    // Blue background to match LabeledSwitch theme.
    lv_obj_set_style_bg_color(container_, lv_color_hex(0x0000FF), 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);

    // Create label (top left).
    label_ = lv_label_create(container_);
    lv_label_set_text(label_, label_text_.c_str());
    lv_obj_align(label_, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_color(label_, lv_color_hex(0xFFFFFF), 0); // White text.

    // Create switch (top right).
    switch_ = lv_switch_create(container_);
    lv_obj_align(switch_, LV_ALIGN_TOP_RIGHT, 0, -5);
    lv_obj_set_size(switch_, 50, 25);

    if (initially_enabled_) {
        lv_obj_add_state(switch_, LV_STATE_CHECKED);
    }

    // Create slider (below label/switch).
    slider_ = lv_slider_create(container_);
    lv_obj_align(slider_, LV_ALIGN_TOP_LEFT, 0, 30);
    lv_obj_set_size(slider_, LV_PCT(100), 10);
    lv_slider_set_range(slider_, range_min_, range_max_);
    lv_slider_set_value(slider_, initially_enabled_ ? initial_value_ : 0, LV_ANIM_OFF);

    // Set initial color (slider always interactive for auto-enable).
    if (!initially_enabled_) {
        lv_obj_set_style_bg_color(slider_, lv_color_hex(0x808080), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(slider_, lv_color_hex(0x808080), LV_PART_KNOB);
    }
    else {
        lv_obj_set_style_bg_color(slider_, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(slider_, lv_palette_main(LV_PALETTE_BLUE), LV_PART_KNOB);
    }

    // Create value label (above slider).
    valueLabel_ = lv_label_create(container_);
    double scaledValue = (initially_enabled_ ? initial_value_ : 0) * value_scale_;
    char buf[32];
    snprintf(buf, sizeof(buf), value_format_.c_str(), scaledValue);
    lv_label_set_text(valueLabel_, buf);
    lv_obj_align_to(valueLabel_, slider_, LV_ALIGN_OUT_TOP_MID, 0, -5);
    lv_obj_set_style_text_font(valueLabel_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(valueLabel_, lv_color_hex(0xFFFFFF), 0); // White text.

    // Create persistent state for callbacks.
    ToggleSliderState* state =
        new ToggleSliderState{ slider_,          valueLabel_,      switch_,
                               value_scale_,     value_format_,    initial_value_,
                               default_value_,   slider_callback_, slider_user_data_,
                               toggle_callback_, toggle_user_data_ };

    // Set user_data on widgets so user callbacks can access it.
    if (toggle_user_data_) {
        lv_obj_set_user_data(switch_, toggle_user_data_);
    }
    if (slider_user_data_) {
        lv_obj_set_user_data(slider_, slider_user_data_);
    }

    // Set up callbacks.
    lv_obj_add_event_cb(switch_, toggleSliderSwitchCallback, LV_EVENT_VALUE_CHANGED, state);
    lv_obj_add_event_cb(slider_, toggleSliderValueCallback, LV_EVENT_VALUE_CHANGED, state);
    lv_obj_add_event_cb(slider_, toggleSliderAutoEnableCallback, LV_EVENT_PRESSED, state);

    // Cleanup callback to free state.
    lv_obj_add_event_cb(container_, toggleSliderDeleteCallback, LV_EVENT_DELETE, state);

    return Result<lv_obj_t*, std::string>::okay(container_);
}

Result<lv_obj_t*, std::string> LVGLBuilder::ToggleSliderBuilder::build()
{
    return createToggleSlider();
}

lv_obj_t* LVGLBuilder::ToggleSliderBuilder::buildOrLog()
{
    auto result = createToggleSlider();
    if (result.isError()) {
        spdlog::error("ToggleSliderBuilder: {}", result.error());
        return nullptr;
    }
    return result.value();
}

// ============================================================================
// CollapsiblePanelBuilder Implementation.
// ============================================================================

LVGLBuilder::CollapsiblePanelBuilder::CollapsiblePanelBuilder(lv_obj_t* parent)
    : parent_(parent),
      container_(nullptr),
      header_(nullptr),
      content_(nullptr),
      title_label_(nullptr),
      indicator_(nullptr),
      size_(LV_PCT(30), LV_SIZE_CONTENT),
      is_expanded_(true),
      bg_color_(0x303030),
      header_color_(0x404040),
      toggle_callback_(nullptr),
      user_data_(nullptr)
{}

LVGLBuilder::CollapsiblePanelBuilder& LVGLBuilder::CollapsiblePanelBuilder::title(const char* text)
{
    if (text) {
        title_text_ = text;
    }
    return *this;
}

LVGLBuilder::CollapsiblePanelBuilder& LVGLBuilder::CollapsiblePanelBuilder::size(
    int width, int height)
{
    size_ = Size(width, height);
    return *this;
}

LVGLBuilder::CollapsiblePanelBuilder& LVGLBuilder::CollapsiblePanelBuilder::size(const Size& sz)
{
    size_ = sz;
    return *this;
}

LVGLBuilder::CollapsiblePanelBuilder& LVGLBuilder::CollapsiblePanelBuilder::initiallyExpanded(
    bool expanded)
{
    is_expanded_ = expanded;
    return *this;
}

LVGLBuilder::CollapsiblePanelBuilder& LVGLBuilder::CollapsiblePanelBuilder::backgroundColor(
    uint32_t color)
{
    bg_color_ = color;
    return *this;
}

LVGLBuilder::CollapsiblePanelBuilder& LVGLBuilder::CollapsiblePanelBuilder::headerColor(
    uint32_t color)
{
    header_color_ = color;
    return *this;
}

LVGLBuilder::CollapsiblePanelBuilder& LVGLBuilder::CollapsiblePanelBuilder::onToggle(
    lv_event_cb_t cb, void* user_data)
{
    toggle_callback_ = cb;
    user_data_ = user_data;
    return *this;
}

Result<lv_obj_t*, std::string> LVGLBuilder::CollapsiblePanelBuilder::build()
{
    if (!parent_) {
        std::string error = "CollapsiblePanelBuilder: parent cannot be null";
        spdlog::error(error);
        return Result<lv_obj_t*, std::string>::error(error);
    }

    auto result = createCollapsiblePanel();
    if (result.isError()) {
        return result;
    }

    spdlog::debug(
        "CollapsiblePanelBuilder: Successfully created collapsible panel '{}'", title_text_);

    return Result<lv_obj_t*, std::string>::okay(container_);
}

lv_obj_t* LVGLBuilder::CollapsiblePanelBuilder::buildOrLog()
{
    auto result = build();
    if (result.isError()) {
        spdlog::error("CollapsiblePanelBuilder: {}", result.error());
        return nullptr;
    }
    return result.value();
}

Result<lv_obj_t*, std::string> LVGLBuilder::CollapsiblePanelBuilder::createCollapsiblePanel()
{
    // Create main container.
    container_ = lv_obj_create(parent_);
    if (!container_) {
        std::string error = "CollapsiblePanelBuilder: Failed to create container";
        spdlog::error(error);
        return Result<lv_obj_t*, std::string>::error(error);
    }

    lv_obj_set_size(container_, size_.width, size_.height);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        container_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, lv_color_hex(bg_color_), 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);

    // Create clickable header.
    header_ = lv_obj_create(container_);
    if (!header_) {
        std::string error = "CollapsiblePanelBuilder: Failed to create header";
        spdlog::error(error);
        return Result<lv_obj_t*, std::string>::error(error);
    }

    lv_obj_set_size(header_, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(header_, 8, 0);
    lv_obj_set_style_bg_color(header_, lv_color_hex(header_color_), 0);
    lv_obj_set_style_bg_opa(header_, LV_OPA_COVER, 0);
    lv_obj_add_flag(header_, LV_OBJ_FLAG_CLICKABLE);

    // Create expand/collapse indicator.
    indicator_ = lv_label_create(header_);
    if (!indicator_) {
        std::string error = "CollapsiblePanelBuilder: Failed to create indicator";
        spdlog::error(error);
        return Result<lv_obj_t*, std::string>::error(error);
    }

    lv_label_set_text(indicator_, is_expanded_ ? LV_SYMBOL_DOWN : LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(indicator_, lv_color_hex(0xFFFFFF), 0);

    // Create title label.
    title_label_ = lv_label_create(header_);
    if (!title_label_) {
        std::string error = "CollapsiblePanelBuilder: Failed to create title label";
        spdlog::error(error);
        return Result<lv_obj_t*, std::string>::error(error);
    }

    lv_label_set_text(title_label_, title_text_.c_str());
    lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_left(title_label_, 8, 0);

    // Create content area.
    content_ = lv_obj_create(container_);
    if (!content_) {
        std::string error = "CollapsiblePanelBuilder: Failed to create content area";
        spdlog::error(error);
        return Result<lv_obj_t*, std::string>::error(error);
    }

    lv_obj_set_size(content_, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        content_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(content_, 4, 0);
    lv_obj_set_style_pad_all(content_, 8, 0);
    lv_obj_set_style_bg_opa(content_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content_, 0, 0);

    // Set initial state.
    if (!is_expanded_) {
        lv_obj_add_flag(content_, LV_OBJ_FLAG_HIDDEN);
    }

    // Allocate and store state for the header click callback.
    PanelState* state = new PanelState{ content_, indicator_, is_expanded_ };
    lv_obj_set_user_data(header_, state);

    // Setup header click event.
    lv_obj_add_event_cb(header_, onHeaderClick, LV_EVENT_CLICKED, nullptr);

    // Setup optional user callback (called after internal state change).
    if (toggle_callback_) {
        lv_obj_add_event_cb(header_, toggle_callback_, LV_EVENT_CLICKED, user_data_);
    }

    // Add delete callback to clean up allocated state.
    lv_obj_add_event_cb(
        header_,
        [](lv_event_t* e) {
            lv_obj_t* header = static_cast<lv_obj_t*>(lv_event_get_target(e));
            PanelState* state = static_cast<PanelState*>(lv_obj_get_user_data(header));
            delete state;
        },
        LV_EVENT_DELETE,
        nullptr);

    return Result<lv_obj_t*, std::string>::okay(container_);
}

void LVGLBuilder::CollapsiblePanelBuilder::onHeaderClick(lv_event_t* e)
{
    lv_obj_t* header = static_cast<lv_obj_t*>(lv_event_get_target(e));
    PanelState* state = static_cast<PanelState*>(lv_obj_get_user_data(header));

    if (!state || !state->content || !state->indicator) {
        spdlog::warn("CollapsiblePanelBuilder: Invalid panel state in header click");
        return;
    }

    // Toggle expanded state.
    state->is_expanded = !state->is_expanded;

    // Update indicator symbol.
    lv_label_set_text(state->indicator, state->is_expanded ? LV_SYMBOL_DOWN : LV_SYMBOL_RIGHT);

    // Show/hide content with animation.
    if (state->is_expanded) {
        lv_obj_remove_flag(state->content, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(state->content, LV_OBJ_FLAG_HIDDEN);
    }

    spdlog::debug(
        "CollapsiblePanelBuilder: Panel toggled to {}",
        state->is_expanded ? "expanded" : "collapsed");
}

// ============================================================================
// Static Factory Methods.
// ============================================================================

LVGLBuilder::SliderBuilder LVGLBuilder::slider(lv_obj_t* parent)
{
    return SliderBuilder(parent);
}

LVGLBuilder::ButtonBuilder LVGLBuilder::button(lv_obj_t* parent)
{
    return ButtonBuilder(parent);
}

LVGLBuilder::LabelBuilder LVGLBuilder::label(lv_obj_t* parent)
{
    return LabelBuilder(parent);
}

LVGLBuilder::DropdownBuilder LVGLBuilder::dropdown(lv_obj_t* parent)
{
    return DropdownBuilder(parent);
}

LVGLBuilder::LabeledSwitchBuilder LVGLBuilder::labeledSwitch(lv_obj_t* parent)
{
    return LabeledSwitchBuilder(parent);
}

LVGLBuilder::ToggleSliderBuilder LVGLBuilder::toggleSlider(lv_obj_t* parent)
{
    return ToggleSliderBuilder(parent);
}

LVGLBuilder::CollapsiblePanelBuilder LVGLBuilder::collapsiblePanel(lv_obj_t* parent)
{
    return CollapsiblePanelBuilder(parent);
}
