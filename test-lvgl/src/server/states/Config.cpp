#include "../../ui/ui_builders/LVGLEventBuilder.h"
#include "../StateMachine.h"
#include "State.h"
#include <memory>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace State {

struct ConfigUI {
    lv_obj_t* container = nullptr;
    lv_obj_t* titleLabel = nullptr;
    lv_obj_t* backBtn = nullptr;
    lv_obj_t* worldTypeLabel = nullptr;
    lv_obj_t* worldTypeBtnMatrix = nullptr;
    
    ~ConfigUI() {
        if (container) {
            lv_obj_del(container);
        }
    }
};

static std::unique_ptr<ConfigUI> configUI;

void Config::onEnter(DirtSimStateMachine& dsm) {
    spdlog::info("Config: Creating configuration UI");
    
    // Check if LVGL is initialized (won't be in unit tests)
    if (!lv_is_initialized()) {
        spdlog::warn("Config: LVGL not initialized, skipping UI creation");
        return;
    }
    
    configUI = std::make_unique<ConfigUI>();
    EventRouter* router = &dsm.getEventRouter();
    
    // Create container.
    configUI->container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(configUI->container, 600, 400);
    lv_obj_center(configUI->container);
    
    // Title.
    configUI->titleLabel = lv_label_create(configUI->container);
    lv_label_set_text(configUI->titleLabel, "Settings");
    lv_obj_set_style_text_font(configUI->titleLabel, &lv_font_montserrat_20, 0);
    lv_obj_align(configUI->titleLabel, LV_ALIGN_TOP_MID, 0, 10);
    
    // Back button.
    auto backBtnBuilder = LVGLEventBuilder::button(configUI->container, router);
    backBtnBuilder.text("Back to Menu");
    backBtnBuilder.size(150, 40);
    backBtnBuilder.position(10, 10, LV_ALIGN_TOP_LEFT);
    backBtnBuilder.onClick([]() {
        // Return to main menu.
        return Event{StartSimulationCommand{}}; // Hack: reuse this to go back.
    });
    configUI->backBtn = backBtnBuilder.buildOrLog();
    
    // World type selection.
    configUI->worldTypeLabel = lv_label_create(configUI->container);
    lv_label_set_text(configUI->worldTypeLabel, "Physics System:");
    lv_obj_align(configUI->worldTypeLabel, LV_ALIGN_TOP_LEFT, 20, 80);
    
    // TODO: Add more configuration options.
    
    spdlog::info("Config: UI created");
}

void Config::onExit(DirtSimStateMachine& /*dsm. */) {
    spdlog::info("Config: Cleaning up UI");
    configUI.reset();
}

State::Any Config::onEvent(const StartSimulationCommand& /*cmd*/, DirtSimStateMachine& /*dsm. */) {
    // Hack: using this event to go back to menu.
    return MainMenu{};
}

} // namespace State.
} // namespace DirtSim.