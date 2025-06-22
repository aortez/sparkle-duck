#pragma once

#include "MaterialType.h"
#include "WorldInterface.h"
#include <concepts>
#include <cstdint>
#include <string>
#include <variant>

/**
 * @brief Event definitions for the dual-path event system.
 *
 * Includes events needed to connect the state machine to current UI callbacks.
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
 * @brief Advance simulation by one timestep.
 */
struct AdvanceSimulationCommand {
    static constexpr const char* name() { return "AdvanceSimulationCommand"; }
};

/**
 * @brief Reset simulation to initial state.
 */
struct ResetSimulationCommand {
    static constexpr const char* name() { return "ResetSimulationCommand"; }
};

/**
 * @brief Switch between WorldA and WorldB.
 */
struct SwitchWorldTypeCommand {
    WorldType worldType;
    static constexpr const char* name() { return "SwitchWorldTypeCommand"; }
};

/**
 * @brief Save world to file.
 */
struct SaveWorldCommand {
    std::string filepath;
    static constexpr const char* name() { return "SaveWorldCommand"; }
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
 * @brief Set gravity enabled state.
 */
struct SetGravityCommand {
    bool enabled;
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
 * @brief Toggle debug visualization.
 */
struct ToggleDebugCommand {
    static constexpr const char* name() { return "ToggleDebugCommand"; }
};

/**
 * @brief Toggle cursor force visualization.
 */
struct ToggleForceCommand {
    static constexpr const char* name() { return "ToggleForceCommand"; }
};

/**
 * @brief Toggle cohesion physics.
 */
struct ToggleCohesionCommand {
    static constexpr const char* name() { return "ToggleCohesionCommand"; }
};

/**
 * @brief Toggle adhesion physics.
 */
struct ToggleAdhesionCommand {
    static constexpr const char* name() { return "ToggleAdhesionCommand"; }
};

/**
 * @brief Toggle time history tracking.
 */
struct ToggleTimeHistoryCommand {
    static constexpr const char* name() { return "ToggleTimeHistoryCommand"; }
};

// =================================================================
// UI CONTROL EVENTS
// =================================================================

/**
 * @brief Capture screenshot.
 */
struct CaptureScreenshotCommand {
    static constexpr const char* name() { return "CaptureScreenshotCommand"; }
};

/**
 * @brief Exit application.
 */
struct QuitApplicationCommand {
    static constexpr const char* name() { return "QuitApplicationCommand"; }
};

/**
 * @brief Print ASCII diagram of world state.
 */
struct PrintAsciiDiagramCommand {
    static constexpr const char* name() { return "PrintAsciiDiagramCommand"; }
};

// =================================================================
// MATERIAL SELECTION (From MaterialPicker)
// =================================================================

/**
 * @brief Change selected material type.
 */
struct SelectMaterialCommand {
    MaterialType material;
    static constexpr const char* name() { return "SelectMaterialCommand"; }
};

// =================================================================
// STATE TRANSITION EVENTS
// =================================================================

/**
 * @brief Transition to configuration state.
 */
struct OpenConfigCommand {
    static constexpr const char* name() { return "OpenConfigCommand"; }
};

/**
 * @brief Initialization complete.
 */
struct InitCompleteEvent {
    static constexpr const char* name() { return "InitCompleteEvent"; }
};

// =================================================================
// EVENT VARIANT
// =================================================================

/**
 * @brief Variant containing all event types needed for UI integration.
 */
using Event = std::variant<
    // Immediate events
    GetFPSCommand,
    GetSimStatsCommand,
    PauseCommand,
    ResumeCommand,

    // Simulation control
    StartSimulationCommand,
    AdvanceSimulationCommand,
    ResetSimulationCommand,
    SwitchWorldTypeCommand,
    SaveWorldCommand,

    // Mouse events
    MouseDownEvent,
    MouseMoveEvent,
    MouseUpEvent,

    // Physics parameters
    SetGravityCommand,
    SetElasticityCommand,
    SetTimescaleCommand,
    ToggleDebugCommand,
    ToggleForceCommand,
    ToggleCohesionCommand,
    ToggleAdhesionCommand,
    ToggleTimeHistoryCommand,

    // UI control
    CaptureScreenshotCommand,
    QuitApplicationCommand,
    PrintAsciiDiagramCommand,
    SelectMaterialCommand,

    // State transitions
    OpenConfigCommand,
    InitCompleteEvent>;

/**
 * @brief Helper to get event name from variant.
 */
inline std::string getEventName(const Event& event)
{
    return std::visit([](auto&& e) { return std::string(e.name()); }, event);
}