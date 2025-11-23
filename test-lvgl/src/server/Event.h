#pragma once

#include "api/CellGet.h"
#include "api/CellSet.h"
#include "api/DiagramGet.h"
#include "api/Exit.h"
#include "api/GravitySet.h"
#include "api/PerfStatsGet.h"
#include "api/PhysicsSettingsGet.h"
#include "api/PhysicsSettingsSet.h"
#include "api/RenderFormatSet.h"
#include "api/Reset.h"
#include "api/ScenarioConfigSet.h"
#include "api/SeedAdd.h"
#include "api/SimRun.h"
#include "api/SpawnDirtBall.h"
#include "api/StateGet.h"
#include "api/StatusGet.h"
#include "api/TimerStatsGet.h"
#include "api/WorldResize.h"
#include "core/MaterialType.h"
#include "core/SimulationStats.h"
#include <chrono>
#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace DirtSim {
namespace Server {

/**
 * @brief Event definitions for the server state machine.
 *
 * Includes all simulation control, physics parameters, and API commands.
 */

// =================================================================
// EVENT NAME CONCEPT
// =================================================================

/**
 * @brief Concept for events that have a name() method.
 */
template <typename T>
concept HasEventName = requires {
    { T::name() } -> std::convertible_to<const char*>;
};

// Note: UiUpdateEvent is defined in core/api/UiUpdateEvent.h (shared between server and UI).

// =================================================================
// IMMEDIATE EVENTS (UI Thread, Low Latency)
// =================================================================

/**
 * @brief Query current frames per second.
 */
struct GetFPSCommand {
    static constexpr const char* name() { return "GetFPSCommand"; }
};

/**
 * @brief Query simulation statistics.
 */
struct GetSimStatsCommand {
    static constexpr const char* name() { return "GetSimStatsCommand"; }
};

/**
 * @brief Pause the simulation.
 */
struct PauseCommand {
    static constexpr const char* name() { return "PauseCommand"; }
};

/**
 * @brief Resume the simulation.
 */
struct ResumeCommand {
    static constexpr const char* name() { return "ResumeCommand"; }
};

// =================================================================
// QUEUED EVENTS (Simulation Thread)
// =================================================================

/**
 * @brief Start simulation from menu.
 */
struct StartSimulationCommand {
    static constexpr const char* name() { return "StartSimulationCommand"; }
};

/**
 * @brief Reset simulation to initial state.
 */
struct ResetSimulationCommand {
    static constexpr const char* name() { return "ResetSimulationCommand"; }
};

/**
 * @brief Resize world to new dimensions.
 */
struct ResizeWorldCommand {
    uint32_t width;
    uint32_t height;
    static constexpr const char* name() { return "ResizeWorldCommand"; }
};

/**
 * @brief Apply a scenario to the world.
 */
struct ApplyScenarioCommand {
    std::string scenarioName;
    static constexpr const char* name() { return "ApplyScenarioCommand"; }
};

/**
 * @brief Save world to file.
 */
struct SaveWorldCommand {
    std::string filepath;
    static constexpr const char* name() { return "SaveWorldCommand"; }
};

/**
 * @brief Load world from file.
 */
struct LoadWorldCommand {
    std::string filepath;
    static constexpr const char* name() { return "LoadWorldCommand"; }
};

// =================================================================
// ADVANCED FEATURES (Time Control & Special Operations)
// =================================================================

/**
 * @brief Step simulation backward by one timestep.
 */
struct StepBackwardCommand {
    static constexpr const char* name() { return "StepBackwardCommand"; }
};

/**
 * @brief Step simulation forward by one timestep.
 */
struct StepForwardCommand {
    static constexpr const char* name() { return "StepForwardCommand"; }
};

/**
 * @brief Toggle time reversal mode on/off.
 */
struct ToggleTimeReversalCommand {
    static constexpr const char* name() { return "ToggleTimeReversalCommand"; }
};

/**
 * @brief Set simulation timestep parameter.
 */
struct SetTimestepCommand {
    double timestep_value;
    static constexpr const char* name() { return "SetTimestepCommand"; }
};

// =================================================================
// MOUSE EVENTS (From drawAreaEventCb)
// =================================================================

/**
 * @brief Mouse button pressed.
 */
struct MouseDownEvent {
    int pixelX;
    int pixelY;
    static constexpr const char* name() { return "MouseDownEvent"; }
};

/**
 * @brief Mouse moved while button pressed.
 */
struct MouseMoveEvent {
    int pixelX;
    int pixelY;
    static constexpr const char* name() { return "MouseMoveEvent"; }
};

/**
 * @brief Mouse button released.
 */
struct MouseUpEvent {
    int pixelX;
    int pixelY;
    static constexpr const char* name() { return "MouseUpEvent"; }
};

// =================================================================
// PHYSICS PARAMETER EVENTS (From UI sliders/buttons)
// =================================================================

/**
 * @brief Set gravity strength.
 */
struct SetGravityCommand {
    double gravity;
    static constexpr const char* name() { return "SetGravityCommand"; }
};

/**
 * @brief Set elasticity factor.
 */
struct SetElasticityCommand {
    double elasticity;
    static constexpr const char* name() { return "SetElasticityCommand"; }
};

/**
 * @brief Set simulation timescale.
 */
struct SetTimescaleCommand {
    double timescale;
    static constexpr const char* name() { return "SetTimescaleCommand"; }
};

/**
 * @brief Set dynamic pressure strength.
 */
struct SetDynamicStrengthCommand {
    double strength;
    static constexpr const char* name() { return "SetDynamicStrengthCommand"; }
};

/**
 * @brief Set pressure scale factor.
 */
struct SetPressureScaleCommand {
    double scale;
    static constexpr const char* name() { return "SetPressureScaleCommand"; }
};

/**
 * @brief Set World pressure scale factor.
 */
struct SetPressureScaleWorldBCommand {
    double scale;
    static constexpr const char* name() { return "SetPressureScaleWorldBCommand"; }
};

/**
 * @brief Set contact friction strength factor (surface-to-surface friction).
 */
struct SetContactFrictionStrengthCommand {
    double strength;
    static constexpr const char* name() { return "SetContactFrictionStrengthCommand"; }
};

/**
 * @brief Set COM cohesion range.
 */
struct SetCOMCohesionRangeCommand {
    uint32_t range;
    static constexpr const char* name() { return "SetCOMCohesionRangeCommand"; }
};

/**
 * @brief Set air resistance strength.
 */
struct SetAirResistanceCommand {
    double strength;
    static constexpr const char* name() { return "SetAirResistanceCommand"; }
};

/**
 * @brief Set hydrostatic pressure strength.
 */
struct SetHydrostaticPressureStrengthCommand {
    double strength;
    static constexpr const char* name() { return "SetHydrostaticPressureStrengthCommand"; }
};

/**
 * @brief Set dynamic pressure strength.
 */
struct SetDynamicPressureStrengthCommand {
    double strength;
    static constexpr const char* name() { return "SetDynamicPressureStrengthCommand"; }
};

/**
 * @brief Set rain rate.
 */
struct SetRainRateCommand {
    double rate;
    static constexpr const char* name() { return "SetRainRateCommand"; }
};

/**
 * @brief Toggle cohesion force physics.
 */
struct ToggleCohesionForceCommand {
    static constexpr const char* name() { return "ToggleCohesionForceCommand"; }
};

/**
 * @brief Toggle time history tracking.
 */
struct ToggleTimeHistoryCommand {
    static constexpr const char* name() { return "ToggleTimeHistoryCommand"; }
};

// =================================================================
// MATERIAL & WORLD CONTROLS
// =================================================================

struct SetCellSizeCommand {
    double size;
    static constexpr const char* name() { return "SetCellSizeCommand"; }
};

struct SetFragmentationCommand {
    double factor;
    static constexpr const char* name() { return "SetFragmentationCommand"; }
};

struct ToggleWallsCommand {
    static constexpr const char* name() { return "ToggleWallsCommand"; }
};

struct ToggleWaterColumnCommand {
    static constexpr const char* name() { return "ToggleWaterColumnCommand"; }
};

struct ToggleLeftThrowCommand {
    static constexpr const char* name() { return "ToggleLeftThrowCommand"; }
};

struct ToggleRightThrowCommand {
    static constexpr const char* name() { return "ToggleRightThrowCommand"; }
};

struct ToggleQuadrantCommand {
    static constexpr const char* name() { return "ToggleQuadrantCommand"; }
};

struct ToggleFrameLimitCommand {
    static constexpr const char* name() { return "ToggleFrameLimitCommand"; }
};

// =================================================================
// UI CONTROL EVENTS
// =================================================================

struct CaptureScreenshotCommand {
    static constexpr const char* name() { return "CaptureScreenshotCommand"; }
};

struct QuitApplicationCommand {
    static constexpr const char* name() { return "QuitApplicationCommand"; }
};

struct PrintAsciiDiagramCommand {
    static constexpr const char* name() { return "PrintAsciiDiagramCommand"; }
};

struct SpawnDirtBallCommand {
    static constexpr const char* name() { return "SpawnDirtBallCommand"; }
};

/**
 * @brief Forward-declarable Event wrapper class.
 *
 * Wraps the variant to enable forward declaration in headers,
 * reducing compilation dependencies.
 */
class Event {
public:
    using Variant = std::variant<
        // Immediate events.
        GetFPSCommand,
        GetSimStatsCommand,
        PauseCommand,
        ResumeCommand,

        // Simulation control.
        StartSimulationCommand,
        ApplyScenarioCommand,
        ResetSimulationCommand,
        ResizeWorldCommand,
        SaveWorldCommand,
        LoadWorldCommand,
        StepBackwardCommand,
        StepForwardCommand,
        ToggleTimeReversalCommand,
        SetTimestepCommand,

        // Mouse events.
        MouseDownEvent,
        MouseMoveEvent,
        MouseUpEvent,

        // Physics parameters.
        SetGravityCommand,
        SetElasticityCommand,
        SetTimescaleCommand,
        SetDynamicStrengthCommand,
        SetPressureScaleCommand,
        SetPressureScaleWorldBCommand,
        SetContactFrictionStrengthCommand,
        SetCOMCohesionRangeCommand,
        SetAirResistanceCommand,
        SetHydrostaticPressureStrengthCommand,
        SetDynamicPressureStrengthCommand,
        SetRainRateCommand,
        ToggleCohesionForceCommand,
        ToggleTimeHistoryCommand,

        // Material & world controls.
        SetCellSizeCommand,
        SetFragmentationCommand,
        ToggleWallsCommand,
        ToggleWaterColumnCommand,
        ToggleLeftThrowCommand,
        ToggleRightThrowCommand,
        ToggleQuadrantCommand,
        ToggleFrameLimitCommand,

        // UI control.
        CaptureScreenshotCommand,
        QuitApplicationCommand,
        PrintAsciiDiagramCommand,
        SpawnDirtBallCommand,
        SelectMaterialCommand,

        // API commands (network/remote control).
        DirtSim::Api::CellGet::Cwc,
        DirtSim::Api::CellSet::Cwc,
        DirtSim::Api::DiagramGet::Cwc,
        DirtSim::Api::Exit::Cwc,
        DirtSim::Api::GravitySet::Cwc,
        DirtSim::Api::PerfStatsGet::Cwc,
        DirtSim::Api::PhysicsSettingsGet::Cwc,
        DirtSim::Api::PhysicsSettingsSet::Cwc,
        DirtSim::Api::RenderFormatSet::Cwc,
        DirtSim::Api::Reset::Cwc,
        DirtSim::Api::ScenarioConfigSet::Cwc,
        DirtSim::Api::SeedAdd::Cwc,
        DirtSim::Api::SimRun::Cwc,
        DirtSim::Api::SpawnDirtBall::Cwc,
        DirtSim::Api::StateGet::Cwc,
        DirtSim::Api::StatusGet::Cwc,
        DirtSim::Api::TimerStatsGet::Cwc,
        DirtSim::Api::WorldResize::Cwc,

        // State transitions.
        OpenConfigCommand,
        InitCompleteEvent>;

    // Constructor from any event type.
    template <typename T>
    Event(T&& event) : variant_(std::forward<T>(event))
    {}

    // Default constructor.
    Event() = default;

    // Accessor for the underlying variant.
    Variant& getVariant() { return variant_; }
    const Variant& getVariant() const { return variant_; }

private:
    Variant variant_;
};

/**
 * @brief Helper to get event name from Event wrapper.
 */
inline std::string getEventName(const Event& event)
{
    return std::visit([](auto&& e) { return std::string(e.name()); }, event.getVariant());
}

} // namespace Server
} // namespace DirtSim
