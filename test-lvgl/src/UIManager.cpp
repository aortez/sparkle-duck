#include "UIManager.h"
#include <spdlog/spdlog.h>

namespace DirtSim {

UIManager::UIManager(lv_disp_t* display) : display(display) {
    if (!display) {
        spdlog::error("UIManager initialized with null display");
        return;
    }
    
    // Initialize with the default screen
    currentScreen = lv_disp_get_scr_act(display);
    spdlog::info("UIManager initialized with display");
}

UIManager::~UIManager() {
    spdlog::info("UIManager cleanup started");
    
    // Clean up any screens we created (not the default one)
    if (simulationScreen && simulationScreen != lv_disp_get_scr_act(display)) {
        cleanupScreen(simulationScreen);
    }
    if (mainMenuScreen && mainMenuScreen != lv_disp_get_scr_act(display)) {
        cleanupScreen(mainMenuScreen);
    }
    if (configScreen && configScreen != lv_disp_get_scr_act(display)) {
        cleanupScreen(configScreen);
    }
    
    spdlog::info("UIManager cleanup completed");
}

lv_obj_t* UIManager::getSimulationContainer() {
    if (!display) return nullptr;
    
    simulationScreen = ensureScreen(simulationScreen, "simulation");
    transitionToScreen(simulationScreen);
    return simulationScreen;
}

lv_obj_t* UIManager::getMainMenuContainer() {
    if (!display) return nullptr;
    
    mainMenuScreen = ensureScreen(mainMenuScreen, "main_menu");
    transitionToScreen(mainMenuScreen);
    return mainMenuScreen;
}

lv_obj_t* UIManager::getConfigContainer() {
    if (!display) return nullptr;
    
    configScreen = ensureScreen(configScreen, "config");
    transitionToScreen(configScreen);
    return configScreen;
}

void UIManager::clearCurrentContainer() {
    if (currentScreen) {
        lv_obj_clean(currentScreen);
        spdlog::debug("Cleared current container");
    }
}

void UIManager::transitionToScreen(lv_obj_t* screen, bool animate) {
    if (!screen || screen == currentScreen) {
        return;
    }
    
    if (animate) {
        lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
    } else {
        lv_scr_load(screen);
    }
    
    currentScreen = screen;
    spdlog::debug("Transitioned to screen");
}

lv_obj_t* UIManager::ensureScreen(lv_obj_t*& screen, const char* name) {
    if (!screen) {
        screen = lv_obj_create(NULL);
        if (screen) {
            spdlog::debug("Created {} screen", name);
        } else {
            spdlog::error("Failed to create {} screen", name);
        }
    }
    return screen;
}

void UIManager::cleanupScreen(lv_obj_t*& screen) {
    if (screen) {
        lv_obj_del(screen);
        screen = nullptr;
        spdlog::debug("Cleaned up screen");
    }
}

} // namespace DirtSim