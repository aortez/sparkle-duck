#pragma once

#include "StateForward.h"
#include "ui/SimPlayground.h"
#include "ui/rendering/DisplayStreamer.h"
#include "ui/state-machine/Event.h"
#include <memory>

namespace DirtSim {

struct WorldData;

namespace Ui {

class SimPlayground;

namespace State {

/**
 * @brief Simulation running state - active display and interaction.
 */
struct SimRunning {
    std::unique_ptr<WorldData> worldData;              // Local copy of world data for rendering.
    std::unique_ptr<SimPlayground> playground_;        // Coordinates all UI components.
    std::unique_ptr<DisplayStreamer> displayStreamer_; // Streams display to web clients.

    // UI-local draw mode toggles.
    bool debugDrawEnabled = false;

    // UI FPS tracking.
    std::chrono::steady_clock::time_point lastFrameTime;
    double measuredUiFps = 0.0; // Instantaneous UI frame rate.
    double smoothedUiFps = 0.0; // Exponentially smoothed UI FPS for display.
    uint64_t skippedFrames = 0; // Count of skipped frames.

    // Round-trip timing (state_get request → UiUpdateEvent received).
    std::chrono::steady_clock::time_point lastStateGetSentTime;
    double lastRoundTripMs = 0.0;
    double smoothedRoundTripMs = 0.0; // EMA smoothed round-trip time.
    uint64_t updateCount = 0;         // Count of received world updates.
    bool stateGetPending = false;     // Track if a state_get request is awaiting response.

    void onEnter(StateMachine& sm);
    void onExit(StateMachine& sm);

    Any onEvent(const PhysicsSettingsReceivedEvent& evt, StateMachine& sm);
    Any onEvent(const ServerDisconnectedEvent& evt, StateMachine& sm);
    Any onEvent(const UiApi::DisplayStreamStart::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::DisplayStreamStop::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::DrawDebugToggle::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::Exit::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::MouseDown::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::MouseMove::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::MouseUp::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::PixelRendererToggle::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::RenderModeSelect::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::SimPause::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiUpdateEvent& evt, StateMachine& sm);

    static constexpr const char* name() { return "SimRunning"; }
};

} // namespace State
} // namespace Ui
} // namespace DirtSim
