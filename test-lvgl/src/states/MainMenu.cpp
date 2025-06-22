#include "State.h"
#include "../DirtSimStateMachine.h"
#include "../ui/LVGLEventBuilder.h"
#include <spdlog/spdlog.h>
#include <memory>

namespace DirtSim {
namespace State {

// UI components for MainMenu
struct MainMenuUI {
    lv_obj_t* container = nullptr;
    lv_obj_t* titleLabel = nullptr;
    lv_obj_t* startBtn = nullptr;
    lv_obj_t* configBtn = nullptr;
    lv_obj_t* demoBtn = nullptr;
    lv_obj_t* quitBtn = nullptr;
    
    ~MainMenuUI() {
        if (container) {
            lv_obj_del(container);
        }
    }
};

// Static storage for UI (since state structs are value types)
static std::unique_ptr<MainMenuUI> menuUI;

void MainMenu::onEnter(DirtSimStateMachine& dsm) {
    spdlog::info("MainMenu: Creating UI");
    
    // Check if LVGL is initialized (won't be in unit tests)
    if (!lv_is_initialized()) {
        spdlog::warn("MainMenu: LVGL not initialized, skipping UI creation");
        return;
    }
    
    menuUI = std::make_unique<MainMenuUI>();
    EventRouter* router = &dsm.getEventRouter();
    
    // Create main container
    menuUI->container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(menuUI->container, 400, 500);
    lv_obj_center(menuUI->container);
    
    // Title
    menuUI->titleLabel = lv_label_create(menuUI->container);
    lv_label_set_text(menuUI->titleLabel, "Dirt Sim");
    lv_obj_set_style_text_font(menuUI->titleLabel, &lv_font_montserrat_24, 0);
    lv_obj_align(menuUI->titleLabel, LV_ALIGN_TOP_MID, 0, 20);
    
    // Start button
    auto startBtnBuilder = LVGLEventBuilder::button(menuUI->container, router);
    startBtnBuilder.text("Start Simulation");
    startBtnBuilder.size(200, 50);
    startBtnBuilder.position(0, -60, LV_ALIGN_CENTER);
    startBtnBuilder.onClick(Event{StartSimulationCommand{}});
    menuUI->startBtn = startBtnBuilder.buildOrLog();
    
    // Config button
    auto configBtnBuilder = LVGLEventBuilder::button(menuUI->container, router);
    configBtnBuilder.text("Settings");
    configBtnBuilder.size(200, 50);
    configBtnBuilder.position(0, 0, LV_ALIGN_CENTER);
    configBtnBuilder.onClick(Event{OpenConfigCommand{}});
    menuUI->configBtn = configBtnBuilder.buildOrLog();
    
    // Demo button
    auto demoBtnBuilder = LVGLEventBuilder::button(menuUI->container, router);
    demoBtnBuilder.text("Demo Mode");
    demoBtnBuilder.size(200, 50);
    demoBtnBuilder.position(0, 60, LV_ALIGN_CENTER);
    demoBtnBuilder.onClick([]() { 
        spdlog::info("Demo mode not yet implemented");
        return Event{StartSimulationCommand{}}; // For now, just start sim
    });
    menuUI->demoBtn = demoBtnBuilder.buildOrLog();
    
    // Quit button
    auto quitBtnBuilder = LVGLEventBuilder::button(menuUI->container, router);
    quitBtnBuilder.text("Quit");
    quitBtnBuilder.size(200, 50);
    quitBtnBuilder.position(0, 120, LV_ALIGN_CENTER);
    quitBtnBuilder.onClick(Event{QuitApplicationCommand{}});
    menuUI->quitBtn = quitBtnBuilder.buildOrLog();
}

void MainMenu::onExit(DirtSimStateMachine& /*dsm*/) {
    spdlog::info("MainMenu: Cleaning up UI");
    menuUI.reset();
}

State::Any MainMenu::onEvent(const StartSimulationCommand& /*cmd*/, DirtSimStateMachine& /*dsm*/) {
    spdlog::info("MainMenu: Starting simulation");
    
    // UI will be created by SimRunning state
    return SimRunning{};
}

State::Any MainMenu::onEvent(const OpenConfigCommand& /*cmd*/, DirtSimStateMachine& /*dsm*/) {
    spdlog::info("MainMenu: Opening configuration");
    return Config{};
}

State::Any MainMenu::onEvent(const SelectMaterialCommand& cmd, DirtSimStateMachine& dsm) {
    dsm.getSharedState().setSelectedMaterial(cmd.material);
    spdlog::debug("MainMenu: Selected material {}", static_cast<int>(cmd.material));
    return *this;
}

} // namespace State
} // namespace DirtSim