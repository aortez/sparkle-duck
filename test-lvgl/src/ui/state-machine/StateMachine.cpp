#include "StateMachine.h"
#include "states/State.h"
#include "network/WebSocketServer.h"
#include "network/WebSocketClient.h"
#include <spdlog/spdlog.h>

namespace DirtSim {
namespace Ui {

StateMachine::StateMachine(_lv_display_t* disp, uint16_t wsPort)
    : display(disp)
{
    spdlog::info("Ui::StateMachine initialized in state: {}", getCurrentStateName());

    // Create WebSocket server for accepting remote commands.
    wsServer_ = new WebSocketServer(*this, wsPort);
    wsServer_->start();
    spdlog::info("Ui::StateMachine: WebSocket server listening on port {}", wsPort);

    // Create WebSocket client for connecting to DSSM server.
    wsClient_ = new WebSocketClient();
    spdlog::info("Ui::StateMachine: WebSocket client created (not yet connected)");
}

StateMachine::~StateMachine()
{
    spdlog::info("Ui::StateMachine shutting down from state: {}", getCurrentStateName());

    // Stop and clean up WebSocket client.
    if (wsClient_) {
        wsClient_->disconnect();
        delete wsClient_;
        wsClient_ = nullptr;
    }

    // Stop and clean up WebSocket server.
    if (wsServer_) {
        wsServer_->stop();
        delete wsServer_;
        wsServer_ = nullptr;
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

void StateMachine::handleEvent(const Event& event)
{
    spdlog::debug("Ui::StateMachine: Handling event: {}", getEventName(event));

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
                        spdlog::warn(
                            "Ui::StateMachine: State {} does not handle event {}",
                            State::getCurrentStateName(fsmState),
                            getEventName(Event{ evt }));

                        // If this is an API command with sendResponse, send error.
                        if constexpr (requires { evt.sendResponse(std::declval<typename std::decay_t<decltype(evt)>::Response>()); }) {
                            auto errorMsg = std::string("Command not supported in state: ") + State::getCurrentStateName(fsmState);
                            using EventType = std::decay_t<decltype(evt)>;
                            using ResponseType = typename EventType::Response;
                            evt.sendResponse(ResponseType::error(ApiError(errorMsg)));
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
