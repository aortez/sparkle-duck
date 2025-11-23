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

// Forward declaration of wrapper (definition in State.h after state includes).
class Any;

} // namespace State
} // namespace Server
} // namespace DirtSim
