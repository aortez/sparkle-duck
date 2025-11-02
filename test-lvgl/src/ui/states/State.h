#pragma once

#include <string>
#include <variant>

namespace DirtSim {
namespace Ui {

class StateMachine;

namespace State {

struct Startup;
struct MainMenu;
struct SimRunning;
struct Paused;
struct Shutdown;

using Any = std::variant<
    Startup,
    MainMenu,
    SimRunning,
    Paused,
    Shutdown
>;

std::string getCurrentStateName(const Any& state);

} // namespace State
} // namespace Ui
} // namespace DirtSim
