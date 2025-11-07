#pragma once

#include "DrawDebugToggle.h"
#include "Exit.h"
#include "MouseDown.h"
#include "MouseMove.h"
#include "MouseUp.h"
#include "Screenshot.h"
#include "SimPause.h"
#include "SimRun.h"
#include <variant>

namespace DirtSim {
namespace Ui {

/**
 * @brief Variant containing all UI API command types.
 */
using UiApiCommand = std::variant<
    UiApi::DrawDebugToggle::Command,
    UiApi::Exit::Command,
    UiApi::MouseDown::Command,
    UiApi::MouseMove::Command,
    UiApi::MouseUp::Command,
    UiApi::Screenshot::Command,
    UiApi::SimPause::Command,
    UiApi::SimRun::Command
>;

} // namespace Ui
} // namespace DirtSim
