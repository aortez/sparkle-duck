#pragma once

#include "DrawDebugToggle.h"
#include "Exit.h"
#include "MouseDown.h"
#include "MouseMove.h"
#include "MouseUp.h"
#include "PixelRendererToggle.h"
#include "Screenshot.h"
#include "SimPause.h"
#include "SimRun.h"
#include "StatusGet.h"
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
    UiApi::PixelRendererToggle::Command,
    UiApi::Screenshot::Command,
    UiApi::SimPause::Command,
    UiApi::SimRun::Command,
    UiApi::StatusGet::Command>;

} // namespace Ui
} // namespace DirtSim
