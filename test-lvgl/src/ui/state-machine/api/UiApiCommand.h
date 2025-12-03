#pragma once

#include "DisplayStreamStart.h"
#include "DisplayStreamStop.h"
#include "DrawDebugToggle.h"
#include "Exit.h"
#include "MouseDown.h"
#include "MouseMove.h"
#include "MouseUp.h"
#include "PixelRendererToggle.h"
#include "RenderModeSelect.h"
#include "Screenshot.h"
#include "SimPause.h"
#include "SimRun.h"
#include "SimStop.h"
#include "StatusGet.h"
#include <variant>

namespace DirtSim {
namespace Ui {

/**
 * @brief Variant containing all UI API command types.
 */
using UiApiCommand = std::variant<
    UiApi::DisplayStreamStart::Command,
    UiApi::DisplayStreamStop::Command,
    UiApi::DrawDebugToggle::Command,
    UiApi::Exit::Command,
    UiApi::MouseDown::Command,
    UiApi::MouseMove::Command,
    UiApi::MouseUp::Command,
    UiApi::PixelRendererToggle::Command,
    UiApi::RenderModeSelect::Command,
    UiApi::Screenshot::Command,
    UiApi::SimPause::Command,
    UiApi::SimRun::Command,
    UiApi::SimStop::Command,
    UiApi::StatusGet::Command>;

} // namespace Ui
} // namespace DirtSim
