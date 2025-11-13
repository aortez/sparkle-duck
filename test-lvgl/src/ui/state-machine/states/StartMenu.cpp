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

    // Create Julia fractal background (allocated on heap, will be deleted by timer cleanup).
    auto* fractal = new JuliaFractal(container, windowWidth, windowHeight);
    spdlog::info("StartMenu: Created fractal background");

    // Add resize event handler to container (catches window resize events).
    lv_obj_add_event_cb(container, onDisplayResized, LV_EVENT_SIZE_CHANGED, fractal);
    spdlog::info("StartMenu: Added resize event handler");

    // Create animation timer.
    animationTimer_ = lv_timer_create(onAnimationTimer, 16, fractal);
    spdlog::info("StartMenu: Started fractal animation timer");

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
}

void StartMenu::onExit(StateMachine& sm)
{
    spdlog::info("StartMenu: Exiting");

    // Stop animation timer and clean up fractal.
    if (animationTimer_) {
        auto* fractal = static_cast<JuliaFractal*>(animationTimer_->user_data);
        lv_timer_del(animationTimer_);

        // IMPORTANT: Remove the resize event handler before deleting the fractal
        // This prevents use-after-free if a resize event occurs after exit
        auto* uiManager = sm.getUiComponentManager();
        if (uiManager) {
            lv_obj_t* container = uiManager->getMainMenuContainer();
            if (container) {
                lv_obj_remove_event_cb(container, onDisplayResized);
                spdlog::info("StartMenu: Removed resize event handler");
            }
        }

        delete fractal;
        animationTimer_ = nullptr;
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
        nlohmann::json cmd = {
            { "command", "sim_run" },
            { "max_frame_ms", 16 }  // Cap at 60 FPS for UI visualization.
        };
        std::string response = wsClient->sendAndReceive(cmd.dump(), 1000);

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

void StartMenu::onAnimationTimer(lv_timer_t* timer)
{
    auto* fractal = static_cast<JuliaFractal*>(timer->user_data);
    if (fractal) {
        fractal->update();
    }
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
    nlohmann::json simRunCmd = {
        { "command", "sim_run" },
        { "max_frame_ms", 16 }  // Cap at 60 FPS for UI visualization.
    };
    bool sent = wsClient->send(simRunCmd.dump());

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
