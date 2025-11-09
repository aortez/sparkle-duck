#pragma once

#include <string>
#include <variant>

namespace DirtSim {
namespace Server {

class StateMachine;

namespace State {

// Forward declarations.
struct Startup;
struct Idle;
struct SimRunning;
struct SimPaused;
struct Shutdown;

// State variant type.
using Any = std::variant<Startup, Idle, SimRunning, SimPaused, Shutdown>;

} // namespace State
} // namespace Server
} // namespace DirtSim
