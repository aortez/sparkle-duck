#include "State.h"
#include "server/api/SimRun.h"
#include "ui/UiComponentManager.h"
#include "ui/rendering/JuliaFractal.h"
#include "ui/state-machine/StateMachine.h"
#include "ui/state-machine/network/WebSocketClient.h"
#include <lvgl/lvgl.h>
#include <lvgl/src/misc/lv_timer_private.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {
namespace State {

void StartMenu::onEnter(StateMachine& sm)
{
    spdlog::info("StartMenu: Connected to server, ready to start simulation");

    // Get main menu container (switches to menu screen).
    auto* uiManager = sm.getUiComponentManager();
    if (!uiManager) return;

    lv_obj_t* container = uiManager->getMainMenuContainer();

    // Get display dimensions for full-screen fractal.
    lv_disp_t* disp = lv_disp_get_default();
    int windowWidth = lv_disp_get_hor_res(disp);
    int windowHeight = lv_disp_get_ver_res(disp);

    // Create Julia fractal background (allocated on heap, deleted in onExit).
    fractal_ = new JuliaFractal(container, windowWidth, windowHeight);
    spdlog::info("StartMenu: Created fractal background (event-driven rendering)");

    // Add resize event handler to container (catches window resize events).
    lv_obj_add_event_cb(container, onDisplayResized, LV_EVENT_SIZE_CHANGED, fractal_);
    spdlog::info("StartMenu: Added resize event handler");

    // Create centered "Start Simulation" button.
    lv_obj_t* startButton = lv_btn_create(container);
    lv_obj_set_size(startButton, 200, 60);
    lv_obj_center(startButton);
    lv_obj_set_user_data(startButton, &sm);
    lv_obj_add_event_cb(startButton, onStartButtonClicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* label = lv_label_create(startButton);
    lv_label_set_text(label, "Start Simulation");
    lv_obj_center(label);

    spdlog::info("StartMenu: Created start button");

    // Create info panel in bottom-left corner.
    infoPanel_ = lv_obj_create(container);
    lv_obj_set_size(infoPanel_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(infoPanel_, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_set_style_pad_all(infoPanel_, 15, 0);
    lv_obj_set_style_bg_opa(infoPanel_, LV_OPA_70, 0);
    lv_obj_set_style_bg_color(infoPanel_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(infoPanel_, 2, 0);
    lv_obj_set_style_border_color(infoPanel_, lv_color_hex(0x404040), 0);
    lv_obj_set_style_radius(infoPanel_, 8, 0);

    // Set flex layout for vertical stacking (label on top, button below).
    lv_obj_set_layout(infoPanel_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(infoPanel_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        infoPanel_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Create info label.
    infoLabel_ = lv_label_create(infoPanel_);
    lv_label_set_text(infoLabel_, "Loading fractal info...");
    lv_obj_set_style_text_color(infoLabel_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(infoLabel_, &lv_font_montserrat_14, 0);

    // Create "Next Fractal" button.
    nextFractalButton_ = lv_btn_create(infoPanel_);
    lv_obj_set_size(nextFractalButton_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(nextFractalButton_, 10, 0);
    lv_obj_set_style_margin_top(nextFractalButton_, 10, 0);
    lv_obj_set_user_data(nextFractalButton_, this);
    lv_obj_add_event_cb(nextFractalButton_, onNextFractalClicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* btnLabel = lv_label_create(nextFractalButton_);
    lv_label_set_text(btnLabel, "Next Fractal");
    lv_obj_center(btnLabel);

    spdlog::info("StartMenu: Created fractal info panel");

    // Create Quit button in top-left corner (same style as SimRunning).
    quitButton_ = lv_btn_create(container);
    lv_obj_set_size(quitButton_, 80, 40);
    lv_obj_align(quitButton_, LV_ALIGN_TOP_LEFT, 20, 20);
    lv_obj_set_style_bg_color(quitButton_, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_user_data(quitButton_, &sm);
    lv_obj_add_event_cb(quitButton_, onQuitButtonClicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* quitLabel = lv_label_create(quitButton_);
    lv_label_set_text(quitLabel, "Quit");
    lv_obj_center(quitLabel);

    spdlog::info("StartMenu: Created Quit button");
}

void StartMenu::onExit(StateMachine& sm)
{
    spdlog::info("StartMenu: Exiting");

    // Clean up fractal.
    if (fractal_) {
        // IMPORTANT: Remove the resize event handler before deleting the fractal.
        // This prevents use-after-free if a resize event occurs after exit.
        auto* uiManager = sm.getUiComponentManager();
        if (uiManager) {
            lv_obj_t* container = uiManager->getMainMenuContainer();
            if (container) {
                lv_obj_remove_event_cb(container, onDisplayResized);
                spdlog::info("StartMenu: Removed resize event handler");
            }
        }

        delete fractal_;
        fractal_ = nullptr;
        spdlog::info("StartMenu: Cleaned up fractal");
    }

    // Screen switch will clean up other widgets automatically.
}

void StartMenu::onStartButtonClicked(lv_event_t* e)
{
    auto* sm = static_cast<StateMachine*>(
        lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e))));
    if (!sm) return;

    spdlog::info("StartMenu: Start button clicked, sending sim_run to DSSM");

    // Send sim_run command and wait for response.
    auto* wsClient = sm->getWebSocketClient();
    if (wsClient && wsClient->isConnected()) {
        const DirtSim::Api::SimRun::Command cmd{
            .timestep = 0.016,
            .max_steps = -1,
            .scenario_id = "sandbox",
            .max_frame_ms = 16 // Cap at 60 FPS for UI visualization.
        };

        nlohmann::json json = cmd.toJson();
        json["command"] = DirtSim::Api::SimRun::Command::name();
        std::string response = wsClient->sendAndReceive(json.dump(), 1000);

        if (response.empty()) {
            spdlog::error("StartMenu: No response from sim_run");
            return;
        }

        // Parse response to check if server is now running.
        try {
            nlohmann::json responseJson = nlohmann::json::parse(response);
            if (responseJson.contains("value") && responseJson["value"]["running"] == true) {
                spdlog::info("StartMenu: Server confirmed running, transitioning to SimRunning");
                // Queue transition event.
                sm->queueEvent(ServerRunningConfirmedEvent{});
            }
            else {
                spdlog::warn("StartMenu: Server not running after sim_run");
            }
        }
        catch (const std::exception& ex) {
            spdlog::error("StartMenu: Failed to parse sim_run response: {}", ex.what());
        }
    }
    else {
        spdlog::error("StartMenu: Cannot start simulation, not connected to DSSM");
    }
}

void StartMenu::updateAnimations()
{
    if (fractal_) {
        fractal_->update();

        // Update info label with current fractal parameters (~1/sec to reduce overhead).
        if (infoLabel_) {
            labelUpdateCounter_++;
            if (labelUpdateCounter_ >= 60) { // Update ~1/sec at 60fps.
                labelUpdateCounter_ = 0;

                double cReal = fractal_->getCReal();
                double cImag = fractal_->getCImag();
                const char* regionName = fractal_->getRegionName();

                // Get all iteration values atomically to prevent race conditions.
                int minIter, currentIter, maxIter;
                fractal_->getIterationInfo(minIter, currentIter, maxIter);

                double fps = fractal_->getDisplayFps();

                // Periodic logging every 100 frames to track iteration values.
                updateFrameCount_++;
                if (updateFrameCount_ >= 100) {
                    spdlog::info(
                        "StartMenu: Fractal info - Region: {}, Iterations: [{}-{}], current: {}, "
                        "FPS: {:.1f}",
                        regionName,
                        minIter,
                        maxIter,
                        currentIter,
                        fps);
                    updateFrameCount_ = 0;
                }

                // Format Julia constant with proper sign for imaginary part.
                char cConstant[64];
                if (cImag >= 0) {
                    snprintf(cConstant, sizeof(cConstant), "%.4f + %.4fi", cReal, cImag);
                }
                else {
                    snprintf(cConstant, sizeof(cConstant), "%.4f - %.4fi", cReal, -cImag);
                }

                // Build multi-line info text.
                char infoText[512];
                snprintf(
                    infoText,
                    sizeof(infoText),
                    "Region: %s\n"
                    "Julia constant: c = %s\n"
                    "Iterations: [%d-%d], current: %d\n"
                    "FPS: %.1f",
                    regionName,
                    cConstant,
                    minIter,
                    maxIter,
                    currentIter,
                    fps);

                lv_label_set_text(infoLabel_, infoText);
            }
        }
    }
}

void StartMenu::onNextFractalClicked(lv_event_t* e)
{
    auto* startMenu = static_cast<StartMenu*>(
        lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e))));
    if (!startMenu || !startMenu->fractal_) return;

    spdlog::info("StartMenu: Next fractal button clicked");
    startMenu->fractal_->advanceToNextFractal();
}

void StartMenu::onQuitButtonClicked(lv_event_t* e)
{
    auto* sm = static_cast<StateMachine*>(
        lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e))));
    if (!sm) return;

    spdlog::info("StartMenu: Quit button clicked");

    // Queue UI-local exit event (works in all states).
    UiApi::Exit::Cwc cwc;
    cwc.callback = [](auto&&) {}; // No response needed.
    sm->queueEvent(cwc);
}

void StartMenu::onDisplayResized(lv_event_t* e)
{
    auto* fractal = static_cast<JuliaFractal*>(lv_event_get_user_data(e));
    if (!fractal) return;

    // Get new display dimensions.
    lv_disp_t* disp = lv_disp_get_default();
    int newWidth = lv_disp_get_hor_res(disp);
    int newHeight = lv_disp_get_ver_res(disp);

    spdlog::info("StartMenu: Display resized to {}x{}, updating fractal", newWidth, newHeight);

    // Resize the fractal to match.
    fractal->resize(newWidth, newHeight);
}

State::Any StartMenu::onEvent(const ServerRunningConfirmedEvent& /*evt*/, StateMachine& /*sm*/)
{
    spdlog::info("StartMenu: Server confirmed running, transitioning to SimRunning");
    return SimRunning{};
}

State::Any StartMenu::onEvent(const FrameReadyNotification& evt, StateMachine& /*sm*/)
{
    spdlog::info(
        "StartMenu: Received frame_ready (step {}), server already running simulation",
        evt.stepNumber);
    spdlog::info("StartMenu: Transitioning to SimRunning to display visualization");

    // Server already has a running simulation - transition to SimRunning to render it.
    SimRunning newState; // Default initialization handles all fields.
    return newState;     // Will initialize worldData, renderer, and controls in onEnter.
}

State::Any StartMenu::onEvent(const ServerDisconnectedEvent& evt, StateMachine& /*sm*/)
{
    spdlog::warn("StartMenu: Server disconnected (reason: {})", evt.reason);
    spdlog::info("StartMenu: Transitioning back to Disconnected");

    // Lost connection - go back to Disconnected state.
    return Disconnected{};
}

State::Any StartMenu::onEvent(const UiApi::Exit::Cwc& cwc, StateMachine& /*sm*/)
{
    spdlog::info("StartMenu: Exit command received, shutting down");

    // Send success response.
    cwc.sendResponse(UiApi::Exit::Response::okay(std::monostate{}));

    // Transition to Shutdown state.
    return Shutdown{};
}

State::Any StartMenu::onEvent(const UiApi::SimRun::Cwc& cwc, StateMachine& sm)
{
    spdlog::info("StartMenu: SimRun command received");

    // Get WebSocket client to send command to DSSM.
    auto* wsClient = sm.getWebSocketClient();
    if (!wsClient || !wsClient->isConnected()) {
        spdlog::error("StartMenu: Not connected to DSSM server");
        cwc.sendResponse(UiApi::SimRun::Response::error(ApiError("Not connected to DSSM server")));
        return StartMenu{};
    }

    // Send sim_run command to DSSM server.
    const DirtSim::Api::SimRun::Command cmd{
        .timestep = 0.016,
        .max_steps = -1,
        .scenario_id = "sandbox",
        .max_frame_ms = 16 // Cap at 60 FPS for UI visualization.
    };
    bool sent = wsClient->sendCommand(cmd);

    if (!sent) {
        spdlog::error("StartMenu: Failed to send sim_run to DSSM");
        cwc.sendResponse(
            UiApi::SimRun::Response::error(ApiError("Failed to send command to DSSM")));
        return StartMenu{};
    }

    spdlog::info("StartMenu: Sent sim_run to DSSM, transitioning to SimRunning");

    // Send OK response.
    cwc.sendResponse(UiApi::SimRun::Response::okay({ true }));

    // Transition to SimRunning state.
    return SimRunning{};
}

} // namespace State
} // namespace Ui
} // namespace DirtSim
