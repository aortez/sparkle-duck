#pragma once

#include <string>
#include <variant>

namespace DirtSim {
namespace Ui {

class StateMachine;

namespace State {

// Forward declarations.
struct Disconnected;
struct Paused;
struct Shutdown;
struct SimRunning;
struct StartMenu;
struct Startup;

// State variant type.
using Any = std::variant<Disconnected, Paused, Shutdown, SimRunning, StartMenu, Startup>;

} // namespace State
} // namespace Ui
} // namespace DirtSim
