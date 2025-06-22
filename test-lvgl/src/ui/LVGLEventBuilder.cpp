#include "LVGLEventBuilder.h"
#include "../World.h"  // For PressureMode enum
#include <spdlog/spdlog.h>

namespace DirtSim {

// ===== Static Factory Methods =====

LVGLEventBuilder::SliderBuilder LVGLEventBuilder::slider(lv_obj_t* parent, EventRouter* router) {
    return SliderBuilder(parent).withEventRouter(router);
}

LVGLEventBuilder::ButtonBuilder LVGLEventBuilder::button(lv_obj_t* parent, EventRouter* router) {
    return ButtonBuilder(parent).withEventRouter(router);
}

LVGLEventBuilder::ButtonMatrixBuilder LVGLEventBuilder::buttonMatrix(lv_obj_t* parent, EventRouter* router) {
    return ButtonMatrixBuilder(parent).withEventRouter(router);
}

LVGLEventBuilder::DropdownBuilder LVGLEventBuilder::dropdown(lv_obj_t* parent, EventRouter* router) {
    return DropdownBuilder(parent).withEventRouter(router);
}

LVGLEventBuilder::DrawAreaBuilder LVGLEventBuilder::drawArea(lv_obj_t* parent, EventRouter* router) {
    return DrawAreaBuilder(parent).withEventRouter(router);
}

// ===== Generic Event Callback =====

void LVGLEventBuilder::eventCallback(lv_event_t* e) {
    auto* dataHolder = static_cast<std::shared_ptr<std::function<void()>>*>(lv_event_get_user_data(e));
    if (dataHolder && *dataHolder) {
        try {
            (**dataHolder)();
        } catch (const std::exception& ex) {
            spdlog::error("Exception in LVGL event callback: {}", ex.what());
        }
    }
}

// ===== SliderBuilder Implementation =====

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::withEventRouter(EventRouter* router) {
    eventRouter_ = router;
    return *this;
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onValueChange(std::function<Event(int32_t)> handler) {
    if (!eventRouter_) {
        spdlog::error("SliderBuilder: EventRouter not set! Use withEventRouter() first.");
        return *this;
    }
    
    eventHandler_ = std::make_shared<std::function<Event(int32_t)>>(handler);
    
    auto callbackFunc = std::make_shared<std::function<void()>>([this, handler]() {
        if (auto* slider = getSlider()) {
            int32_t value = lv_slider_get_value(slider);
            Event event = handler(value);
            eventRouter_->routeEvent(event);
        }
    });
    
    // Use the base class callback method with our generic callback
    callback([](lv_event_t* e) { eventCallback(e); }, createCallbackData(callbackFunc));
    events(LV_EVENT_VALUE_CHANGED);
    
    return *this;
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onTimescaleChange() {
    return onValueChange([](int32_t value) {
        double timescale = value / 100.0;  // Convert 0-100 to 0.0-1.0
        return Event{SetTimescaleCommand{timescale}};
    });
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onElasticityChange() {
    return onValueChange([](int32_t value) {
        double elasticity = value / 100.0;  // Convert 0-100 to 0.0-1.0
        return Event{SetElasticityCommand{elasticity}};
    });
}

// ===== ButtonBuilder Implementation =====

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::withEventRouter(EventRouter* router) {
    eventRouter_ = router;
    return *this;
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onClick(Event event) {
    return onClick([event]() { return event; });
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onClick(std::function<Event()> handler) {
    if (!eventRouter_) {
        spdlog::error("ButtonBuilder: EventRouter not set! Use withEventRouter() first.");
        return *this;
    }
    
    eventHandler_ = std::make_shared<std::function<Event()>>(handler);
    
    auto callbackFunc = std::make_shared<std::function<void()>>([this, handler]() {
        Event event = handler();
        eventRouter_->routeEvent(event);
    });
    
    callback([](lv_event_t* e) { eventCallback(e); }, createCallbackData(callbackFunc));
    events(LV_EVENT_CLICKED);
    
    return *this;
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onToggle(Event checkedEvent, Event uncheckedEvent) {
    if (!eventRouter_) {
        spdlog::error("ButtonBuilder: EventRouter not set! Use withEventRouter() first.");
        return *this;
    }
    
    toggleEvents_ = std::make_shared<std::pair<Event, Event>>(checkedEvent, uncheckedEvent);
    toggle(true);  // Enable toggle mode
    
    auto callbackFunc = std::make_shared<std::function<void()>>([this, checkedEvent, uncheckedEvent]() {
        if (auto* btn = getButton()) {
            bool isChecked = lv_obj_has_state(btn, LV_STATE_CHECKED);
            Event event = isChecked ? checkedEvent : uncheckedEvent;
            eventRouter_->routeEvent(event);
        }
    });
    
    callback([](lv_event_t* e) { eventCallback(e); }, createCallbackData(callbackFunc));
    events(LV_EVENT_VALUE_CHANGED);
    
    return *this;
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onPauseResume() {
    return onToggle(Event{PauseCommand{}}, Event{ResumeCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onReset() {
    return onClick(Event{ResetSimulationCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onGravityToggle() {
    // We need to track gravity state somehow, for now just toggle with true/false
    return onToggle(Event{SetGravityCommand{true}}, Event{SetGravityCommand{false}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onPrintAscii() {
    return onClick(Event{PrintAsciiDiagramCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onDebugToggle() {
    return onClick(Event{ToggleDebugCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onQuit() {
    return onClick(Event{QuitApplicationCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onScreenshot() {
    return onClick(Event{CaptureScreenshotCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onForceToggle() {
    return onClick(Event{ToggleForceCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onCohesionToggle() {
    return onClick(Event{ToggleCohesionCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onAdhesionToggle() {
    return onClick(Event{ToggleAdhesionCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onTimeHistoryToggle() {
    return onClick(Event{ToggleTimeHistoryCommand{}});
}

// ===== ButtonMatrixBuilder Implementation =====

LVGLEventBuilder::ButtonMatrixBuilder& LVGLEventBuilder::ButtonMatrixBuilder::withEventRouter(EventRouter* router) {
    eventRouter_ = router;
    return *this;
}

LVGLEventBuilder::ButtonMatrixBuilder& LVGLEventBuilder::ButtonMatrixBuilder::onSelect(std::function<Event(uint32_t)> handler) {
    if (!eventRouter_) {
        spdlog::error("ButtonMatrixBuilder: EventRouter not set! Use withEventRouter() first.");
        return *this;
    }
    
    // Store the handler for later use in build()
    eventHandler_ = std::make_shared<std::function<Event(uint32_t)>>(handler);
    return *this;
}

LVGLEventBuilder::ButtonMatrixBuilder& LVGLEventBuilder::ButtonMatrixBuilder::onWorldTypeSelect() {
    return onSelect([](uint32_t index) {
        // Map button index to WorldType
        ::WorldType type = (index == 0) ? ::WorldType::RulesA : ::WorldType::RulesB;
        return Event{SwitchWorldTypeCommand{type}};
    });
}

// Builder configuration methods
LVGLEventBuilder::ButtonMatrixBuilder& LVGLEventBuilder::ButtonMatrixBuilder::map(const char** btnMap) {
    btnMap_ = btnMap;
    return *this;
}

LVGLEventBuilder::ButtonMatrixBuilder& LVGLEventBuilder::ButtonMatrixBuilder::size(int width, int height) {
    size_ = std::make_pair(width, height);
    return *this;
}

LVGLEventBuilder::ButtonMatrixBuilder& LVGLEventBuilder::ButtonMatrixBuilder::position(int x, int y, lv_align_t align) {
    position_ = std::make_tuple(x, y, align);
    return *this;
}

LVGLEventBuilder::ButtonMatrixBuilder& LVGLEventBuilder::ButtonMatrixBuilder::oneChecked(bool enable) {
    oneChecked_ = enable;
    return *this;
}

LVGLEventBuilder::ButtonMatrixBuilder& LVGLEventBuilder::ButtonMatrixBuilder::buttonCtrl(uint16_t btnId, lv_buttonmatrix_ctrl_t ctrl) {
    buttonCtrls_.push_back(std::make_pair(btnId, ctrl));
    return *this;
}

LVGLEventBuilder::ButtonMatrixBuilder& LVGLEventBuilder::ButtonMatrixBuilder::selectedButton(uint16_t btnId) {
    selectedButton_ = btnId;
    return *this;
}

LVGLEventBuilder::ButtonMatrixBuilder& LVGLEventBuilder::ButtonMatrixBuilder::style(lv_style_selector_t selector, std::function<void(lv_style_t*)> styleFunc) {
    styles_.push_back(std::make_pair(selector, styleFunc));
    return *this;
}

Result<lv_obj_t*, std::string> LVGLEventBuilder::ButtonMatrixBuilder::build() {
    if (!parent_) {
        return Result<lv_obj_t*, std::string>::error(std::string("ButtonMatrixBuilder: Parent is null"));
    }
    
    btnMatrix_ = lv_buttonmatrix_create(parent_);
    if (!btnMatrix_) {
        return Result<lv_obj_t*, std::string>::error(std::string("Failed to create button matrix"));
    }
    
    // Apply configurations
    if (btnMap_) {
        lv_buttonmatrix_set_map(btnMatrix_, btnMap_);
    }
    
    if (size_) {
        lv_obj_set_size(btnMatrix_, size_->first, size_->second);
    }
    
    if (position_) {
        lv_obj_align(btnMatrix_, std::get<2>(*position_), std::get<0>(*position_), std::get<1>(*position_));
    }
    
    if (oneChecked_) {
        lv_buttonmatrix_set_one_checked(btnMatrix_, true);
    }
    
    for (const auto& [btnId, ctrl] : buttonCtrls_) {
        lv_buttonmatrix_set_button_ctrl(btnMatrix_, btnId, ctrl);
    }
    
    if (selectedButton_) {
        lv_buttonmatrix_set_selected_button(btnMatrix_, *selectedButton_);
    }
    
    // Apply styles
    for (const auto& [selector, styleFunc] : styles_) {
        lv_style_t* style = new lv_style_t;
        lv_style_init(style);
        styleFunc(style);
        lv_obj_add_style(btnMatrix_, style, selector);
    }
    
    // Set up event handler if configured
    if (eventHandler_ && eventRouter_) {
        auto callbackFunc = std::make_shared<std::function<void()>>([this]() {
            if (btnMatrix_) {
                uint32_t btnIndex = lv_buttonmatrix_get_selected_button(btnMatrix_);
                // LVGL returns 0xFFFF for no selection
                if (btnIndex != 0xFFFF) {
                    Event event = (*eventHandler_)(btnIndex);
                    eventRouter_->routeEvent(event);
                }
            }
        });
        
        lv_obj_add_event_cb(btnMatrix_, eventCallback, LV_EVENT_VALUE_CHANGED, createCallbackData(callbackFunc));
    }
    
    return Result<lv_obj_t*, std::string>::okay(btnMatrix_);
}

lv_obj_t* LVGLEventBuilder::ButtonMatrixBuilder::buildOrLog() {
    auto result = build();
    if (result.isError()) {
        spdlog::error("ButtonMatrixBuilder: {}", result.error());
        return nullptr;
    }
    return result.value();
}

// ===== DropdownBuilder Implementation =====

LVGLEventBuilder::DropdownBuilder& LVGLEventBuilder::DropdownBuilder::withEventRouter(EventRouter* router) {
    eventRouter_ = router;
    return *this;
}

LVGLEventBuilder::DropdownBuilder& LVGLEventBuilder::DropdownBuilder::onValueChange(std::function<Event(uint16_t)> handler) {
    eventHandler_ = std::make_shared<std::function<Event(uint16_t)>>(std::move(handler));
    return *this;
}

LVGLEventBuilder::DropdownBuilder& LVGLEventBuilder::DropdownBuilder::onPressureSystemChange() {
    // TODO: Implement when SetPressureModeCommand is added to the event system
    // For now, pressure system changes will remain as direct manipulation
    spdlog::warn("onPressureSystemChange() not yet implemented - pressure system needs direct manipulation");
    return *this;
}

LVGLEventBuilder::DrawAreaBuilder::DrawAreaBuilder(lv_obj_t* parent) 
    : parent_(parent), drawArea_(nullptr) {}

LVGLEventBuilder::DrawAreaBuilder& LVGLEventBuilder::DrawAreaBuilder::size(int width, int height) {
    size_ = LVGLBuilder::Size(width, height);
    return *this;
}

LVGLEventBuilder::DrawAreaBuilder& LVGLEventBuilder::DrawAreaBuilder::position(int x, int y, lv_align_t align) {
    position_ = LVGLBuilder::Position(x, y, align);
    return *this;
}

LVGLEventBuilder::DrawAreaBuilder& LVGLEventBuilder::DrawAreaBuilder::withEventRouter(EventRouter* router) {
    eventRouter_ = router;
    return *this;
}

LVGLEventBuilder::DrawAreaBuilder& LVGLEventBuilder::DrawAreaBuilder::onMouseEvents() {
    onMouseDown([](int x, int y) { return Event{MouseDownEvent{x, y}}; });
    onMouseMove([](int x, int y) { return Event{MouseMoveEvent{x, y}}; });
    onMouseUp([](int x, int y) { return Event{MouseUpEvent{x, y}}; });
    return *this;
}

LVGLEventBuilder::DrawAreaBuilder& LVGLEventBuilder::DrawAreaBuilder::onMouseDown(std::function<Event(int, int)> handler) {
    mouseDownHandler_ = std::make_shared<std::function<Event(int, int)>>(handler);
    return *this;
}

LVGLEventBuilder::DrawAreaBuilder& LVGLEventBuilder::DrawAreaBuilder::onMouseMove(std::function<Event(int, int)> handler) {
    mouseMoveHandler_ = std::make_shared<std::function<Event(int, int)>>(handler);
    return *this;
}

LVGLEventBuilder::DrawAreaBuilder& LVGLEventBuilder::DrawAreaBuilder::onMouseUp(std::function<Event(int, int)> handler) {
    mouseUpHandler_ = std::make_shared<std::function<Event(int, int)>>(handler);
    return *this;
}

std::pair<int, int> LVGLEventBuilder::DrawAreaBuilder::getRelativeCoords(lv_obj_t* obj, lv_point_t* point) {
    // Get object position
    lv_area_t area;
    lv_obj_get_coords(obj, &area);
    
    // Calculate relative coordinates
    int relX = point->x - area.x1;
    int relY = point->y - area.y1;
    
    return {relX, relY};
}

void LVGLEventBuilder::DrawAreaBuilder::setupMouseEvents() {
    if (!eventRouter_) {
        spdlog::error("DrawAreaBuilder: EventRouter not set!");
        return;
    }
    
    // Mouse down
    if (mouseDownHandler_) {
        auto callbackFunc = std::make_shared<std::function<void()>>([this]() {
            lv_point_t point;
            lv_indev_get_point(lv_indev_get_act(), &point);
            auto [x, y] = getRelativeCoords(drawArea_, &point);
            Event event = (*mouseDownHandler_)(x, y);
            eventRouter_->routeEvent(event);
        });
        
        lv_obj_add_event_cb(drawArea_, eventCallback, LV_EVENT_PRESSED, createCallbackData(callbackFunc));
    }
    
    // Mouse move
    if (mouseMoveHandler_) {
        auto callbackFunc = std::make_shared<std::function<void()>>([this]() {
            lv_point_t point;
            lv_indev_get_point(lv_indev_get_act(), &point);
            auto [x, y] = getRelativeCoords(drawArea_, &point);
            Event event = (*mouseMoveHandler_)(x, y);
            eventRouter_->routeEvent(event);
        });
        
        lv_obj_add_event_cb(drawArea_, eventCallback, LV_EVENT_PRESSING, createCallbackData(callbackFunc));
    }
    
    // Mouse up
    if (mouseUpHandler_) {
        auto callbackFunc = std::make_shared<std::function<void()>>([this]() {
            lv_point_t point;
            lv_indev_get_point(lv_indev_get_act(), &point);
            auto [x, y] = getRelativeCoords(drawArea_, &point);
            Event event = (*mouseUpHandler_)(x, y);
            eventRouter_->routeEvent(event);
        });
        
        lv_obj_add_event_cb(drawArea_, eventCallback, LV_EVENT_RELEASED, createCallbackData(callbackFunc));
    }
}

Result<lv_obj_t*, std::string> LVGLEventBuilder::DrawAreaBuilder::build() {
    if (!parent_) {
        return Result<lv_obj_t*, std::string>::error(std::string("DrawAreaBuilder: Parent is null"));
    }
    
    drawArea_ = lv_obj_create(parent_);
    if (!drawArea_) {
        return Result<lv_obj_t*, std::string>::error(std::string("Failed to create draw area"));
    }
    
    // Set size and position
    lv_obj_set_size(drawArea_, size_.width, size_.height);
    lv_obj_align(drawArea_, position_.align, position_.x, position_.y);
    
    // Make it clickable
    lv_obj_add_flag(drawArea_, LV_OBJ_FLAG_CLICKABLE);
    
    // Set up events
    setupMouseEvents();
    
    return Result<lv_obj_t*, std::string>::okay(drawArea_);
}

lv_obj_t* LVGLEventBuilder::DrawAreaBuilder::buildOrLog() {
    auto result = build();
    if (result.isError()) {
        spdlog::error("DrawAreaBuilder: {}", result.error());
        return nullptr;
    }
    return result.value();
}



} // namespace DirtSim
