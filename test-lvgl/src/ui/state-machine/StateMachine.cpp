#include "StateMachine.h"
#include "core/network/WebSocketService.h"
#include "network/WebSocketClient.h"
#include "network/WebSocketServer.h"
#include "states/State.h"
#include "ui/DisplayCapture.h"
#include "ui/UiComponentManager.h"
#include <chrono>
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

StateMachine::StateMachine(_lv_display_t* disp, uint16_t wsPort) : display(disp)
{
    spdlog::info("Ui::StateMachine initialized in state: {}", getCurrentStateName());

    // Create OLD WebSocket server for accepting remote commands.
    wsServer_ = std::make_unique<WebSocketServer>(*this, wsPort);
    wsServer_->start();
    spdlog::info("Ui::StateMachine: WebSocket server listening on port {}", wsPort);

    // Create OLD WebSocket client for connecting to DSSM server.
    wsClient_ = std::make_unique<WebSocketClient>();
    wsClient_->setEventSink(this); // StateMachine implements EventSink.
    spdlog::info("Ui::StateMachine: WebSocket client created (not yet connected)");

    // Create NEW unified WebSocketService (will replace both above).
    wsService_ = std::make_unique<Network::WebSocketService>();
    setupWebSocketService();
    spdlog::info("Ui::StateMachine: WebSocketService initialized");

    // Create UI manager for LVGL screen/container management.
    uiManager_ = std::make_unique<UiComponentManager>(disp);
    spdlog::info("Ui::StateMachine: UiComponentManager created");
}

void StateMachine::setupWebSocketService()
{
    spdlog::info("Ui::StateMachine: Setting up WebSocketService command handlers...");

    // Register handlers for UI commands that come from CLI (port 7070).
    // All UI commands are queued to the state machine for processing.
    wsService_->registerHandler<UiApi::SimRun::Cwc>(
        [this](UiApi::SimRun::Cwc cwc) { queueEvent(cwc); });
    wsService_->registerHandler<UiApi::SimPause::Cwc>(
        [this](UiApi::SimPause::Cwc cwc) { queueEvent(cwc); });
    wsService_->registerHandler<UiApi::SimStop::Cwc>(
        [this](UiApi::SimStop::Cwc cwc) { queueEvent(cwc); });
    wsService_->registerHandler<UiApi::StatusGet::Cwc>(
        [this](UiApi::StatusGet::Cwc cwc) { queueEvent(cwc); });

    // TODO: Register remaining UI commands (ScreenGrab, DrawDebugToggle, etc.).
    // TODO: Setup client-side connection to server (port 8080).
    // TODO: Setup RenderMessage binary callback for world updates.

    spdlog::info("Ui::StateMachine: WebSocketService handlers registered");
}

StateMachine::~StateMachine()
{
    spdlog::info("Ui::StateMachine shutting down from state: {}", getCurrentStateName());

    // Stop and clean up WebSocket client (unique_ptr handles deletion).
    if (wsClient_) {
        wsClient_->disconnect();
    }

    // Stop and clean up WebSocket server (unique_ptr handles deletion).
    if (wsServer_) {
        wsServer_->stop();
    }
}

void StateMachine::mainLoopRun()
{
    spdlog::info("Starting UI main event loop");

    // Initialize by sending init complete event.
    queueEvent(InitCompleteEvent{});

    // Main event processing loop.
    while (!shouldExit()) {
        processEvents();
        // TODO: LVGL event processing integration.
        // TODO: WebSocket client/server event processing.
    }

    spdlog::info("UI main event loop exiting (shouldExit=true)");
}

void StateMachine::queueEvent(const Event& event)
{
    eventProcessor.enqueueEvent(event);
}

void StateMachine::processEvents()
{
    eventProcessor.processEventsFromQueue(*this);
}

void StateMachine::updateAnimations()
{
    // Track how often main loop runs (debug).
    static int callCount = 0;
    static double lastLogTime = 0.0;
    callCount++;

    double currentTime =
        std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (currentTime - lastLogTime >= 10.0) {
        double loopFps = callCount / (currentTime - lastLogTime);
        spdlog::info("StateMachine: Main loop FPS = {:.1f}", loopFps);
        callCount = 0;
        lastLogTime = currentTime;
    }

    // Delegate to current state (if it has animation updates).
    std::visit(
        [](auto&& state) {
            if constexpr (requires { state.updateAnimations(); }) {
                state.updateAnimations();
            }
        },
        fsmState);
}

void StateMachine::handleEvent(const Event& event)
{
    spdlog::debug("Ui::StateMachine: Handling event: {}", getEventName(event));

    // Handle StatusGet universally (works in all states).
    if (std::holds_alternative<UiApi::StatusGet::Cwc>(event)) {
        spdlog::debug("Ui::StateMachine: Processing StatusGet command");
        auto& cwc = std::get<UiApi::StatusGet::Cwc>(event);

        UiApi::StatusGet::Okay status{
            .state = getCurrentStateName(),
            .connected_to_server = wsClient_ && wsClient_->isConnected(),
            .server_url = "", // TODO: Store and return actual server URL.
            .display_width =
                display ? static_cast<uint32_t>(lv_display_get_horizontal_resolution(display)) : 0U,
            .display_height =
                display ? static_cast<uint32_t>(lv_display_get_vertical_resolution(display)) : 0U,
            .fps = 0.0 // TODO: Track and return actual FPS.
        };

        spdlog::debug("Ui::StateMachine: Sending StatusGet response (state={})", status.state);
        cwc.sendResponse(UiApi::StatusGet::Response::okay(std::move(status)));
        return;
    }

    // Handle ScreenGrab universally (works in all states).
    // Note: Throttling is handled per-client in WebSocketServer before queuing.
    if (std::holds_alternative<UiApi::ScreenGrab::Cwc>(event)) {
        auto& cwc = std::get<UiApi::ScreenGrab::Cwc>(event);

        spdlog::info(
            "Ui::StateMachine: Processing ScreenGrab command (scale={})", cwc.command.scale);

        // Capture display pixels.
        auto screenshotData = captureDisplayPixels(display, cwc.command.scale);
        if (!screenshotData) {
            spdlog::error("Ui::StateMachine: Failed to capture display pixels");
            try {
                cwc.sendResponse(
                    UiApi::ScreenGrab::Response::error(ApiError("Failed to capture display")));
            }
            catch (const std::exception& e) {
                spdlog::warn("Ui::StateMachine: Failed to send error response: {}", e.what());
            }
            return;
        }

        // Encode raw pixels to base64 (no PNG compression).
        std::string base64Pixels = base64Encode(screenshotData->pixels);

        spdlog::info(
            "Ui::StateMachine: ScreenGrab captured {}x{} ({} bytes raw, {} bytes base64)",
            screenshotData->width,
            screenshotData->height,
            screenshotData->pixels.size(),
            base64Pixels.size());
        try {
            cwc.sendResponse(UiApi::ScreenGrab::Response::okay(
                { base64Pixels, screenshotData->width, screenshotData->height }));
        }
        catch (const std::exception& e) {
            spdlog::warn("Ui::StateMachine: Failed to send response: {}", e.what());
        }
        return;
    }

    std::visit(
        [this](auto&& evt) {
            std::visit(
                [this, &evt](auto&& state) -> void {
                    using StateType = std::decay_t<decltype(state)>;

                    if constexpr (requires { state.onEvent(evt, *this); }) {
                        auto newState = state.onEvent(evt, *this);
                        if (!std::holds_alternative<StateType>(newState)) {
                            transitionTo(std::move(newState));
                        }
                        else {
                            // Same state type - move it back into variant to preserve state.
                            fsmState = std::move(newState);
                        }
                    }
                    else {
                        // Handle state-independent events generically.
                        if constexpr (std::is_same_v<std::decay_t<decltype(evt)>, UiUpdateEvent>) {
                            // UiUpdateEvent can arrive in any state (server keeps sending updates).
                            // States that care (SimRunning) have specific handlers.
                            // Other states (Paused, etc.) gracefully ignore without warning.
                            spdlog::info(
                                "Ui::StateMachine: Ignoring UiUpdateEvent in state {}",
                                State::getCurrentStateName(fsmState));
                            // Stay in current state - no transition.
                        }
                        else {
                            spdlog::warn(
                                "Ui::StateMachine: State {} does not handle event {}",
                                State::getCurrentStateName(fsmState),
                                getEventName(Event{ evt }));

                            // If this is an API command with sendResponse, send error.
                            if constexpr (requires {
                                              evt.sendResponse(std::declval<typename std::decay_t<
                                                                   decltype(evt)>::Response>());
                                          }) {
                                auto errorMsg = std::string("Command not supported in state: ")
                                    + State::getCurrentStateName(fsmState);
                                using EventType = std::decay_t<decltype(evt)>;
                                using ResponseType = typename EventType::Response;
                                evt.sendResponse(ResponseType::error(ApiError(errorMsg)));
                            }
                        }
                    }
                },
                fsmState);
        },
        event);
}

std::string StateMachine::getCurrentStateName() const
{
    return State::getCurrentStateName(fsmState);
}

void StateMachine::transitionTo(State::Any newState)
{
    std::string oldStateName = State::getCurrentStateName(fsmState);

    std::visit([this](auto&& state) { callOnExit(state); }, fsmState);

    fsmState = std::move(newState);

    std::string newStateName = State::getCurrentStateName(fsmState);
    spdlog::info("Ui::StateMachine: {} -> {}", oldStateName, newStateName);

    std::visit([this](auto&& state) { callOnEnter(state); }, fsmState);
}

} // namespace Ui
} // namespace DirtSim
