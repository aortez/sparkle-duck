#include "LVGLEventBuilder.h"
#include "../World.h"  // For PressureMode enum.
#include <spdlog/spdlog.h>
#include <cmath>  // For pow()

namespace DirtSim {

// ===== Static Factory Methods =====

LVGLEventBuilder::SliderBuilder LVGLEventBuilder::slider(lv_obj_t* parent, EventRouter* router) {
    return SliderBuilder(parent).withEventRouter(router);
}

LVGLEventBuilder::ButtonBuilder LVGLEventBuilder::button(lv_obj_t* parent, EventRouter* router) {
    return ButtonBuilder(parent).withEventRouter(router);
}

LVGLEventBuilder::SwitchBuilder LVGLEventBuilder::lvSwitch(lv_obj_t* parent, EventRouter* router) {
    return SwitchBuilder(parent).withEventRouter(router);
}

LVGLEventBuilder::ButtonMatrixBuilder LVGLEventBuilder::buttonMatrix(lv_obj_t* parent, EventRouter* router) {
    return ButtonMatrixBuilder(parent).withEventRouter(router);
}

LVGLEventBuilder::DropdownBuilder LVGLEventBuilder::dropdown(lv_obj_t* parent, EventRouter* router) {
    return DropdownBuilder(parent).withEventRouter(router);
}

LVGLEventBuilder::ToggleSliderBuilder LVGLEventBuilder::toggleSlider(lv_obj_t* parent, EventRouter* router) {
    return ToggleSliderBuilder(parent, router);
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

    EventRouter* router = eventRouter_;  // Capture router pointer.

    // Create a callback that gets the slider from the event (not from getSlider()).
    auto callbackFunc = std::make_shared<std::function<void(lv_event_t*)>>([router, handler](lv_event_t* e) {
        // Get slider from event target (not from captured pointer).
        lv_obj_t* slider = lv_event_get_target_obj(e);
        if (slider && router) {
            int32_t value = lv_slider_get_value(slider);
            spdlog::info("SliderBuilder: Slider value changed to {}", value);
            Event event = handler(value);
            spdlog::info("SliderBuilder: Generated event, routing...");
            router->routeEvent(event);
        }
    });

    // Register callback using static wrapper.
    callback([](lv_event_t* e) {
        auto* funcPtr = static_cast<std::shared_ptr<std::function<void(lv_event_t*)>>*>(lv_event_get_user_data(e));
        if (funcPtr && *funcPtr) {
            (**funcPtr)(e);
        }
    }, new std::shared_ptr<std::function<void(lv_event_t*)>>(callbackFunc));
    events(LV_EVENT_VALUE_CHANGED);
    
    return *this;
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onTimescaleChange() {
    // Set up value transform for the label to show timescale instead of slider value
    valueTransform([](int32_t value) {
        // Convert 0-100 to logarithmic scale: 0.1x to 10x with 1.0x at center (50)
        return pow(10.0, (value - 50) / 50.0);
    });
    
    return onValueChange([](int32_t value) {
        // Convert 0-100 to logarithmic scale: 0.1x to 10x with 1.0x at center (50)
        double timescale = pow(10.0, (value - 50) / 50.0);
        return Event{SetTimescaleCommand{timescale}};
    });
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onElasticityChange() {
    return onValueChange([](int32_t value) {
        double elasticity = value / 100.0;  // Convert 0-100 to 0.0-1.0
        return Event{SetElasticityCommand{elasticity}};
    });
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onGravityChange() {
    // Set up value transform for display label.
    valueTransform([](int32_t value) {
        return (value / 100.0) * 9.81;  // Convert -1000 to +1000 → -98.1 to +98.1
    });

    return onValueChange([](int32_t value) {
        double gravity = (value / 100.0) * 9.81;  // -1000 to +1000 → -98.1 to +98.1
        return Event{SetGravityCommand{gravity}};
    });
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onDynamicStrengthChange() {
    return onValueChange([](int32_t value) {
        double strength = value / 100.0;  // Convert 0-300 to 0.0-3.0
        return Event{SetDynamicStrengthCommand{strength}};
    });
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onCohesionForceStrengthChange() {
    valueTransform([](int32_t value) {
        return value / 100.0;  // Convert 0-30000 to 0.0-300.0
    });

    return onValueChange([](int32_t value) {
        double strength = value / 100.0;
        return Event{SetCohesionForceStrengthCommand{strength}};
    });
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onAdhesionStrengthChange() {
    valueTransform([](int32_t value) {
        return value / 100.0;  // Convert 0-1000 to 0.0-10.0
    });

    return onValueChange([](int32_t value) {
        double strength = value / 100.0;
        return Event{SetAdhesionStrengthCommand{strength}};
    });
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onViscosityStrengthChange() {
    valueTransform([](int32_t value) {
        return value / 100.0;  // Convert 0-200 to 0.0-2.0
    });

    return onValueChange([](int32_t value) {
        double strength = value / 100.0;
        return Event{SetViscosityStrengthCommand{strength}};
    });
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onFrictionStrengthChange() {
    valueTransform([](int32_t value) {
        return value / 10.0;  // Convert 0-10 to 0.0-1.0
    });

    return onValueChange([](int32_t value) {
        double strength = value / 10.0;
        return Event{SetFrictionStrengthCommand{strength}};
    });
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onCOMCohesionRangeChange() {
    return onValueChange([](int32_t value) {
        return Event{SetCOMCohesionRangeCommand{static_cast<uint32_t>(value)}};
    });
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onPressureScaleChange() {
    valueTransform([](int32_t value) {
        return value / 100.0;  // Convert 0-1000 to 0.0-10.0
    });

    return onValueChange([](int32_t value) {
        double scale = value / 100.0;
        return Event{SetPressureScaleCommand{scale}};
    });
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onPressureScaleWorldBChange() {
    valueTransform([](int32_t value) {
        return value / 100.0;  // Convert 0-200 to 0.0-2.0
    });

    return onValueChange([](int32_t value) {
        double scale = value / 100.0;
        return Event{SetPressureScaleWorldBCommand{scale}};
    });
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onHydrostaticPressureStrengthChange() {
    valueTransform([](int32_t value) {
        return value / 100.0;  // Convert 0-300 to 0.0-3.0
    });

    return onValueChange([](int32_t value) {
        double strength = value / 100.0;
        return Event{SetHydrostaticPressureStrengthCommand{strength}};
    });
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onAirResistanceChange() {
    valueTransform([](int32_t value) {
        return value / 100.0;  // Convert 0-100 to 0.0-1.0
    });

    return onValueChange([](int32_t value) {
        double strength = value / 100.0;
        return Event{SetAirResistanceCommand{strength}};
    });
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onCellSizeChange() {
    return onValueChange([](int32_t value) {
        return Event{SetCellSizeCommand{static_cast<double>(value)}};
    });
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onFragmentationChange() {
    valueTransform([](int32_t value) {
        return value / 100.0;  // Convert 0-100 to 0.0-1.0
    });

    return onValueChange([](int32_t value) {
        double factor = value / 100.0;
        return Event{SetFragmentationCommand{factor}};
    });
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onRainRateChange() {
    return onValueChange([](int32_t value) {
        return Event{SetRainRateCommand{static_cast<double>(value)}};
    });
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onWaterCohesionChange() {
    valueTransform([](int32_t value) {
        return value / 1000.0;  // Convert 0-1000 to 0.0-1.0
    });

    return onValueChange([](int32_t value) {
        double cohesion = value / 1000.0;
        return Event{SetWaterCohesionCommand{cohesion}};
    });
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onWaterViscosityChange() {
    valueTransform([](int32_t value) {
        return value / 1000.0;  // Convert 0-1000 to 0.0-1.0
    });

    return onValueChange([](int32_t value) {
        double viscosity = value / 1000.0;
        return Event{SetWaterViscosityCommand{viscosity}};
    });
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onWaterPressureThresholdChange() {
    valueTransform([](int32_t value) {
        return value / 100000.0;  // Convert 0-1000 to 0.0-0.01
    });

    return onValueChange([](int32_t value) {
        double threshold = value / 100000.0;
        return Event{SetWaterPressureThresholdCommand{threshold}};
    });
}

LVGLEventBuilder::SliderBuilder& LVGLEventBuilder::SliderBuilder::onWaterBuoyancyChange() {
    valueTransform([](int32_t value) {
        return value / 1000.0;  // Convert 0-1000 to 0.0-1.0
    });

    return onValueChange([](int32_t value) {
        double buoyancy = value / 1000.0;
        return Event{SetWaterBuoyancyCommand{buoyancy}};
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

    EventRouter* router = eventRouter_;  // Capture the router pointer directly.
    auto callbackFunc = std::make_shared<std::function<void()>>([router, handler]() {
        Event event = handler();
        spdlog::debug("LVGLEventBuilder: Button clicked, routing event");
        router->routeEvent(event);
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
    toggle(true);  // Enable toggle mode.

    EventRouter* router = eventRouter_;  // Capture the router pointer directly.

    // We need to store the callback data so it can access the button after it's created.
    // We'll use a custom structure that includes both the events and the router.
    struct ToggleCallbackData {
        EventRouter* router;
        Event checkedEvent;
        Event uncheckedEvent;
    };

    auto* data = new ToggleCallbackData{router, checkedEvent, uncheckedEvent};

    // Set up the callback using the standard LVGL callback mechanism.
    callback([](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
            auto* toggleData = static_cast<ToggleCallbackData*>(lv_event_get_user_data(e));
            lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
            if (toggleData && btn) {
                bool isChecked = lv_obj_has_state(btn, LV_STATE_CHECKED);
                Event event = isChecked ? toggleData->checkedEvent : toggleData->uncheckedEvent;
                // Use info level for better visibility during debugging.
                spdlog::info("Button toggle: isChecked={}, routing {} event",
                            isChecked,
                            isChecked ? "checked (PauseCommand)" : "unchecked (ResumeCommand)");
                toggleData->router->routeEvent(event);
            }
        }
    }, data);

    events(LV_EVENT_VALUE_CHANGED);

    return *this;
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onPauseResume() {
    return onToggle(Event{PauseCommand{}}, Event{ResumeCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onReset() {
    return onClick(Event{ResetSimulationCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onPrintAscii() {
    return onClick(Event{PrintAsciiDiagramCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onDebugToggle() {
    return onClick(Event{ToggleDebugCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onQuit() {
    spdlog::info("LVGLEventBuilder: Setting up Quit button");
    return onClick(Event{QuitApplicationCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onScreenshot() {
    return onClick(Event{CaptureScreenshotCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onTimeHistoryToggle() {
    return onClick(Event{ToggleTimeHistoryCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onStepBackward() {
    return onClick(Event{StepBackwardCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onStepForward() {
    return onClick(Event{StepForwardCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onFrameLimitToggle() {
    return onClick(Event{ToggleFrameLimitCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onLeftThrowToggle() {
    return onClick(Event{ToggleLeftThrowCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onRightThrowToggle() {
    return onClick(Event{ToggleRightThrowCommand{}});
}

LVGLEventBuilder::ButtonBuilder& LVGLEventBuilder::ButtonBuilder::onQuadrantToggle() {
    return onClick(Event{ToggleQuadrantCommand{}});
}

// ===== SwitchBuilder Implementation =====

LVGLEventBuilder::SwitchBuilder& LVGLEventBuilder::SwitchBuilder::withEventRouter(EventRouter* router) {
    eventRouter_ = router;
    return *this;
}

LVGLEventBuilder::SwitchBuilder& LVGLEventBuilder::SwitchBuilder::onToggle(Event checkedEvent, Event uncheckedEvent) {
    if (!eventRouter_) {
        spdlog::error("SwitchBuilder: EventRouter not set! Use withEventRouter() first.");
        return *this;
    }

    toggleEvents_ = std::make_shared<std::pair<Event, Event>>(checkedEvent, uncheckedEvent);
    return *this;
}

LVGLEventBuilder::SwitchBuilder& LVGLEventBuilder::SwitchBuilder::onHydrostaticPressureToggle() {
    return onToggle(
        Event{ToggleHydrostaticPressureCommand{}},
        Event{ToggleHydrostaticPressureCommand{}});
}

LVGLEventBuilder::SwitchBuilder& LVGLEventBuilder::SwitchBuilder::onDynamicPressureToggle() {
    return onToggle(
        Event{ToggleDynamicPressureCommand{}},
        Event{ToggleDynamicPressureCommand{}});
}

LVGLEventBuilder::SwitchBuilder& LVGLEventBuilder::SwitchBuilder::onPressureDiffusionToggle() {
    return onToggle(
        Event{TogglePressureDiffusionCommand{}},
        Event{TogglePressureDiffusionCommand{}});
}

LVGLEventBuilder::SwitchBuilder& LVGLEventBuilder::SwitchBuilder::position(int x, int y, lv_align_t align) {
    position_ = std::make_tuple(x, y, align);
    return *this;
}

LVGLEventBuilder::SwitchBuilder& LVGLEventBuilder::SwitchBuilder::checked(bool isChecked) {
    initialChecked_ = isChecked;
    return *this;
}

lv_obj_t* LVGLEventBuilder::SwitchBuilder::buildOrLog() {
    if (!parent_) {
        spdlog::error("SwitchBuilder: parent cannot be null");
        return nullptr;
    }

    // Create the switch.
    switch_ = lv_switch_create(parent_);
    if (!switch_) {
        spdlog::error("SwitchBuilder: Failed to create switch");
        return nullptr;
    }

    // Set position if specified.
    if (position_.has_value()) {
        auto [x, y, align] = position_.value();
        lv_obj_align(switch_, align, x, y);
    }

    // Set initial checked state.
    if (initialChecked_) {
        lv_obj_add_state(switch_, LV_STATE_CHECKED);
    }

    // Set up event handling if toggle events are configured.
    if (toggleEvents_ && eventRouter_) {
        struct SwitchCallbackData {
            EventRouter* router;
            Event checkedEvent;
            Event uncheckedEvent;
        };

        auto* data = new SwitchCallbackData{
            eventRouter_,
            toggleEvents_->first,
            toggleEvents_->second
        };

        lv_obj_add_event_cb(switch_, [](lv_event_t* e) {
            if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
                auto* switchData = static_cast<SwitchCallbackData*>(lv_event_get_user_data(e));
                lv_obj_t* sw = lv_event_get_target_obj(e);
                if (switchData && sw) {
                    bool isChecked = lv_obj_has_state(sw, LV_STATE_CHECKED);
                    Event event = isChecked ? switchData->checkedEvent : switchData->uncheckedEvent;
                    spdlog::info("Switch toggled: isChecked={}, routing event", isChecked);
                    switchData->router->routeEvent(event);
                }
            }
        }, LV_EVENT_VALUE_CHANGED, data);
    }

    return switch_;
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
    
    // Store the handler for later use in build().
    eventHandler_ = std::make_shared<std::function<Event(uint32_t)>>(handler);
    return *this;
}

LVGLEventBuilder::ButtonMatrixBuilder& LVGLEventBuilder::ButtonMatrixBuilder::onWorldTypeSelect() {
    return onSelect([](uint32_t index) {
        // Map button index to WorldType.
        ::WorldType type = (index == 0) ? ::WorldType::RulesA : ::WorldType::RulesB;
        return Event{SwitchWorldTypeCommand{type}};
    });
}

// Builder configuration methods.
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
    
    // Apply configurations.
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
    
    // Apply styles.
    for (const auto& [selector, styleFunc] : styles_) {
        lv_style_t* style = new lv_style_t;
        lv_style_init(style);
        styleFunc(style);
        lv_obj_add_style(btnMatrix_, style, selector);
    }
    
    // Set up event handler if configured.
    if (eventHandler_ && eventRouter_) {
        auto callbackFunc = std::make_shared<std::function<void()>>([this]() {
            if (btnMatrix_) {
                uint32_t btnIndex = lv_buttonmatrix_get_selected_button(btnMatrix_);
                // LVGL returns 0xFFFF for no selection.
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
    return onValueChange([](uint16_t selectedIndex) {
        WorldInterface::PressureSystem system;
        switch (selectedIndex) {
            case 0: system = WorldInterface::PressureSystem::Original; break;
            case 1: system = WorldInterface::PressureSystem::TopDown; break;
            case 2: system = WorldInterface::PressureSystem::IterativeSettling; break;
            default: system = WorldInterface::PressureSystem::Original; break;
        }
        return Event{SetPressureSystemCommand{system}};
    });
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
    // Get object position.
    lv_area_t area;
    lv_obj_get_coords(obj, &area);
    
    // Calculate relative coordinates.
    int relX = point->x - area.x1;
    int relY = point->y - area.y1;
    
    return {relX, relY};
}

void LVGLEventBuilder::DrawAreaBuilder::setupMouseEvents() {
    if (!eventRouter_) {
        spdlog::error("DrawAreaBuilder: EventRouter not set!");
        return;
    }
    
    // Mouse down.
    if (mouseDownHandler_) {
        // Capture by value to avoid use-after-free when builder is destroyed.
        auto handler = mouseDownHandler_;  // shared_ptr copy keeps handler alive
        auto router = eventRouter_;        // raw pointer copy
        auto area = drawArea_;             // raw pointer copy
        auto callbackFunc = std::make_shared<std::function<void()>>([handler, router, area]() {
            lv_point_t point;
            lv_indev_get_point(lv_indev_get_act(), &point);
            auto [x, y] = DrawAreaBuilder::getRelativeCoords(area, &point);
            Event event = (*handler)(x, y);
            router->routeEvent(event);
        });
        
        lv_obj_add_event_cb(drawArea_, eventCallback, LV_EVENT_PRESSED, createCallbackData(callbackFunc));
    }
    
    // Mouse move.
    if (mouseMoveHandler_) {
        // Capture by value to avoid use-after-free when builder is destroyed.
        auto handler = mouseMoveHandler_;  // shared_ptr copy keeps handler alive
        auto router = eventRouter_;        // raw pointer copy
        auto area = drawArea_;             // raw pointer copy
        auto callbackFunc = std::make_shared<std::function<void()>>([handler, router, area]() {
            lv_point_t point;
            lv_indev_get_point(lv_indev_get_act(), &point);
            auto [x, y] = DrawAreaBuilder::getRelativeCoords(area, &point);
            Event event = (*handler)(x, y);
            router->routeEvent(event);
        });
        
        lv_obj_add_event_cb(drawArea_, eventCallback, LV_EVENT_PRESSING, createCallbackData(callbackFunc));
    }
    
    // Mouse up.
    if (mouseUpHandler_) {
        // Capture by value to avoid use-after-free when builder is destroyed.
        auto handler = mouseUpHandler_;    // shared_ptr copy keeps handler alive
        auto router = eventRouter_;        // raw pointer copy
        auto area = drawArea_;             // raw pointer copy
        auto callbackFunc = std::make_shared<std::function<void()>>([handler, router, area]() {
            lv_point_t point;
            lv_indev_get_point(lv_indev_get_act(), &point);
            auto [x, y] = DrawAreaBuilder::getRelativeCoords(area, &point);
            Event event = (*handler)(x, y);
            router->routeEvent(event);
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
    
    // Set size and position.
    lv_obj_set_size(drawArea_, size_.width, size_.height);
    lv_obj_align(drawArea_, position_.align, position_.x, position_.y);
    
    // Make it clickable.
    lv_obj_add_flag(drawArea_, LV_OBJ_FLAG_CLICKABLE);
    
    // Set up events.
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

// ===== ToggleSliderBuilder Implementation =====

LVGLEventBuilder::ToggleSliderBuilder& LVGLEventBuilder::ToggleSliderBuilder::label(const char* text) {
    labelText_ = text;
    return *this;
}

LVGLEventBuilder::ToggleSliderBuilder& LVGLEventBuilder::ToggleSliderBuilder::position(int x, int y, lv_align_t align) {
    position_ = std::make_tuple(x, y, align);
    return *this;
}

LVGLEventBuilder::ToggleSliderBuilder& LVGLEventBuilder::ToggleSliderBuilder::sliderWidth(int width) {
    sliderWidth_ = width;
    return *this;
}

LVGLEventBuilder::ToggleSliderBuilder& LVGLEventBuilder::ToggleSliderBuilder::range(int min, int max) {
    rangeMin_ = min;
    rangeMax_ = max;
    return *this;
}

LVGLEventBuilder::ToggleSliderBuilder& LVGLEventBuilder::ToggleSliderBuilder::value(int initialValue) {
    initialValue_ = initialValue;
    return *this;
}

LVGLEventBuilder::ToggleSliderBuilder& LVGLEventBuilder::ToggleSliderBuilder::defaultValue(int defValue) {
    defaultValue_ = defValue;
    return *this;
}

LVGLEventBuilder::ToggleSliderBuilder& LVGLEventBuilder::ToggleSliderBuilder::valueScale(double scale) {
    valueScale_ = scale;
    return *this;
}

LVGLEventBuilder::ToggleSliderBuilder& LVGLEventBuilder::ToggleSliderBuilder::valueFormat(const char* format) {
    valueFormat_ = format;
    return *this;
}

LVGLEventBuilder::ToggleSliderBuilder& LVGLEventBuilder::ToggleSliderBuilder::valueLabelOffset(int x, int y) {
    valueLabelOffsetX_ = x;
    valueLabelOffsetY_ = y;
    return *this;
}

LVGLEventBuilder::ToggleSliderBuilder& LVGLEventBuilder::ToggleSliderBuilder::initiallyEnabled(bool enabled) {
    initiallyEnabled_ = enabled;
    return *this;
}

LVGLEventBuilder::ToggleSliderBuilder& LVGLEventBuilder::ToggleSliderBuilder::onValueChange(std::function<Event(double)> handler) {
    eventGenerator_ = handler;
    return *this;
}

// State structure to persist toggle slider state across callbacks.
struct ToggleSliderState {
    EventRouter* eventRouter;
    std::function<Event(double)> eventGenerator;
    lv_obj_t* slider;
    lv_obj_t* valueLabel;
    double valueScale;
    const char* valueFormat;
    int savedValue;      // Last non-zero value for restore.
    int defaultValue;    // Default when no saved value exists.
    int rangeMin;
    int rangeMax;
};

static void toggleSliderSwitchCallback(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;

    ToggleSliderState* state = static_cast<ToggleSliderState*>(lv_event_get_user_data(e));
    if (!state) return;

    lv_obj_t* switch_obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
    bool isEnabled = lv_obj_has_state(switch_obj, LV_STATE_CHECKED);

    if (isEnabled) {
        // Toggle ON: Restore saved value (or use default).
        int valueToRestore = (state->savedValue > 0) ? state->savedValue : state->defaultValue;
        lv_slider_set_value(state->slider, valueToRestore, LV_ANIM_OFF);
        lv_obj_clear_state(state->slider, LV_STATE_DISABLED);

        // Restore blue color for indicator and knob.
        lv_obj_set_style_bg_color(state->slider, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(state->slider, lv_palette_main(LV_PALETTE_BLUE), LV_PART_KNOB);

        // Update value label.
        double scaledValue = valueToRestore * state->valueScale;
        char buf[32];
        snprintf(buf, sizeof(buf), state->valueFormat, scaledValue);
        lv_label_set_text(state->valueLabel, buf);

        // Emit event with restored value.
        if (state->eventRouter && state->eventGenerator) {
            state->eventRouter->routeEvent(state->eventGenerator(scaledValue));
        }

        spdlog::debug("ToggleSlider: Enabled - restored value {} (scaled: {:.2f})", valueToRestore, scaledValue);
    } else {
        // Toggle OFF: Save current value, set to 0, disable slider.
        int currentValue = lv_slider_get_value(state->slider);
        if (currentValue > 0) {
            state->savedValue = currentValue;
        }

        lv_slider_set_value(state->slider, 0, LV_ANIM_OFF);
        lv_obj_add_state(state->slider, LV_STATE_DISABLED);

        // Change indicator and knob to grey when disabled.
        lv_obj_set_style_bg_color(state->slider, lv_color_hex(0x808080), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(state->slider, lv_color_hex(0x808080), LV_PART_KNOB);

        // Update value label to show 0.
        char buf[32];
        snprintf(buf, sizeof(buf), state->valueFormat, 0.0);
        lv_label_set_text(state->valueLabel, buf);

        // Emit event with 0 value (disabled).
        if (state->eventRouter && state->eventGenerator) {
            state->eventRouter->routeEvent(state->eventGenerator(0.0));
        }

        spdlog::debug("ToggleSlider: Disabled - saved value {}, set to 0", state->savedValue);
    }
}

static void toggleSliderValueCallback(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;

    ToggleSliderState* state = static_cast<ToggleSliderState*>(lv_event_get_user_data(e));
    if (!state) return;

    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int value = lv_slider_get_value(slider);
    double scaledValue = value * state->valueScale;

    // Update value label.
    char buf[32];
    snprintf(buf, sizeof(buf), state->valueFormat, scaledValue);
    lv_label_set_text(state->valueLabel, buf);

    // Emit event.
    if (state->eventRouter && state->eventGenerator) {
        state->eventRouter->routeEvent(state->eventGenerator(scaledValue));
    }

    spdlog::trace("ToggleSlider: Value changed to {} (scaled: {:.2f})", value, scaledValue);
}

lv_obj_t* LVGLEventBuilder::ToggleSliderBuilder::buildOrLog() {
    if (!position_.has_value()) {
        spdlog::error("ToggleSliderBuilder: position() must be called before buildOrLog()");
        return nullptr;
    }

    if (!eventGenerator_) {
        spdlog::error("ToggleSliderBuilder: onValueChange() must be called before buildOrLog()");
        return nullptr;
    }

    auto [x, y, align] = position_.value();

    // Create container panel for the toggle slider group.
    lv_obj_t* container = lv_obj_create(parent_);
    lv_obj_set_size(container, sliderWidth_ + 10, 70);  // Width: slider + padding, Height: label+switch+slider.
    lv_obj_align(container, align, x - 5, y - 5);
    lv_obj_set_style_pad_all(container, 5, 0);
    lv_obj_set_style_border_width(container, 1, 0);
    lv_obj_set_style_border_color(container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_radius(container, 3, 0);
    lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);  // Disable scrolling.
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);         // Make non-scrollable.

    // Create label (inside container).
    lv_obj_t* label = lv_label_create(container);
    lv_label_set_text(label, labelText_);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 5, 5);

    // Create switch (same row as label, moved left to fit within bounds, positioned slightly higher).
    lv_obj_t* toggle_switch = lv_switch_create(container);
    lv_obj_align(toggle_switch, LV_ALIGN_TOP_LEFT, sliderWidth_ - 55, 0);  // 0px from top edge.
    if (initiallyEnabled_) {
        lv_obj_add_state(toggle_switch, LV_STATE_CHECKED);
    }

    // Create slider (40px below the label/switch row).
    int sliderY = 45;
    lv_obj_t* slider = lv_slider_create(container);
    lv_obj_align(slider, LV_ALIGN_TOP_LEFT, 5, sliderY);
    lv_obj_set_size(slider, sliderWidth_, 10);
    lv_slider_set_range(slider, rangeMin_, rangeMax_);
    lv_slider_set_value(slider, initiallyEnabled_ ? initialValue_ : 0, LV_ANIM_OFF);

    // Set initial color and state based on initial enabled state.
    if (!initiallyEnabled_) {
        lv_obj_add_state(slider, LV_STATE_DISABLED);
        // Set grey color for disabled state.
        lv_obj_set_style_bg_color(slider, lv_color_hex(0x808080), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(slider, lv_color_hex(0x808080), LV_PART_KNOB);
    } else {
        // Set blue color for enabled state.
        lv_obj_set_style_bg_color(slider, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(slider, lv_palette_main(LV_PALETTE_BLUE), LV_PART_KNOB);
    }

    // Create value label (centered horizontally above slider).
    lv_obj_t* valueLabel = lv_label_create(container);
    double scaledValue = (initiallyEnabled_ ? initialValue_ : 0) * valueScale_;
    char buf[32];
    snprintf(buf, sizeof(buf), valueFormat_, scaledValue);
    lv_label_set_text(valueLabel, buf);
    lv_obj_align_to(valueLabel, slider, LV_ALIGN_OUT_TOP_MID, 0, -5);  // Centered above slider.

    // Allocate persistent state for callbacks.
    ToggleSliderState* state = new ToggleSliderState{
        eventRouter_,
        eventGenerator_,
        slider,
        valueLabel,
        valueScale_,
        valueFormat_,
        initialValue_,  // savedValue initialized to initial value.
        defaultValue_,
        rangeMin_,
        rangeMax_
    };

    // Set up switch callback.
    lv_obj_add_event_cb(toggle_switch, toggleSliderSwitchCallback, LV_EVENT_VALUE_CHANGED, state);

    // Set up slider callback.
    lv_obj_add_event_cb(slider, toggleSliderValueCallback, LV_EVENT_VALUE_CHANGED, state);

    // Clean up state when switch is deleted.
    lv_obj_add_event_cb(toggle_switch, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_DELETE) {
            ToggleSliderState* state = static_cast<ToggleSliderState*>(lv_event_get_user_data(e));
            delete state;
        }
    }, LV_EVENT_DELETE, state);

    spdlog::debug("ToggleSliderBuilder: Created '{}' at ({}, {}) with range [{}, {}]",
                  labelText_, x, y, rangeMin_, rangeMax_);

    return toggle_switch;
}

} // namespace DirtSim.
