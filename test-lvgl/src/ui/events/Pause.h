#pragma once

namespace DirtSim {
namespace Ui {

struct PauseCommand {
    static constexpr const char* name() { return "PauseCommand"; }
};

struct ResumeCommand {
    static constexpr const char* name() { return "ResumeCommand"; }
};

} // namespace Ui
} // namespace DirtSim
