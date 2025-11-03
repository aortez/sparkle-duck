#pragma once

#include "StateForward.h"
#include "../Event.h"
#include "../../rendering/CellRenderer.h"
#include "../../controls/ControlPanel.h"
#include <memory>

namespace DirtSim {

struct WorldData;

namespace Ui {
namespace State {

/**
 * @brief Simulation running state - active display and interaction.
 */
struct SimRunning {
    std::unique_ptr<WorldData> worldData;  // Local copy of world data for rendering.
    std::unique_ptr<CellRenderer> renderer_;  // Manages LVGL canvases for cells.
    std::unique_ptr<ControlPanel> controls_;  // UI controls for interaction.

    void onEnter(StateMachine& sm);
    void onExit(StateMachine& sm);

    Any onEvent(const FrameReadyNotification& evt, StateMachine& sm);
    Any onEvent(const ServerDisconnectedEvent& evt, StateMachine& sm);
    Any onEvent(const UiApi::Exit::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::MouseDown::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::MouseMove::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::MouseUp::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::Screenshot::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiApi::SimPause::Cwc& cwc, StateMachine& sm);
    Any onEvent(const UiUpdateEvent& evt, StateMachine& sm);

    static constexpr const char* name() { return "SimRunning"; }
};

} // namespace State
} // namespace Ui
} // namespace DirtSim
