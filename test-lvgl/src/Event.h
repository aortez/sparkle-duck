#pragma once

#include "MaterialType.h"
#include "SimulationStats.h"
#include "WorldInterface.h"
#include <chrono>
#include <concepts>
#include <cstdint>
#include <optional>
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
// PUSH-BASED UI UPDATE SYSTEM
// =================================================================

/**
 * @brief Physics parameters for UI display.
 * Matches SharedSimState::PhysicsParams structure.
 */
struct PhysicsParams {
    double gravity = 9.81;
    double elasticity = 0.8;
    double timescale = 1.0;
    bool debugEnabled = false;
    bool gravityEnabled = true;
    bool forceVisualizationEnabled = false;
    bool cohesionEnabled = true;
    bool adhesionEnabled = true;
    bool timeHistoryEnabled = false;
};

/**
 * @brief Comprehensive UI update event for push-based updates.
 *
 * This event is pushed from the simulation thread at controlled points
 * and consumed by the UI thread via LVGL timer at ~60fps. It contains
 * all UI-relevant state in a single, thread-safe snapshot.
 */
struct UIUpdateEvent {
    // Sequence tracking.
    uint64_t sequenceNum = 0; ///< Monotonic sequence number for update ordering.

    // Core simulation data.
    uint32_t fps = 0;       ///< Current frames per second.
    uint64_t stepCount = 0; ///< Total simulation steps completed.
    SimulationStats stats;  ///< Comprehensive simulation statistics.

    // Physics parameters.
    PhysicsParams physicsParams; ///< Current physics settings.

    // UI state.
    bool isPaused = false;           ///< Simulation paused state.
    bool debugEnabled = false;       ///< Debug visualization state.
    bool forceEnabled = false;       ///< Force visualization state.
    bool cohesionEnabled = true;     ///< Cohesion physics state.
    bool adhesionEnabled = true;     ///< Adhesion physics state.
    bool timeHistoryEnabled = false; ///< Time history tracking state.

    // World state.
    MaterialType selectedMaterial = MaterialType::DIRT; ///< Currently selected material.
    std::string worldType;                              ///< "WorldA" or "WorldB".

    // Timing.
    std::chrono::steady_clock::time_point timestamp; ///< When update was created.

    // Optimization: dirty flags to indicate what changed.
    struct DirtyFlags {
        bool fps = false;           ///< FPS value changed.
        bool stats = false;         ///< Simulation statistics changed.
        bool physicsParams = false; ///< Physics parameters changed.
        bool uiState = false;       ///< UI toggles changed.
        bool worldState = false;    ///< World type or material changed.
    } dirty;

    static constexpr const char* name() { return "UIUpdateEvent"; }
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
 * @brief Set dynamic pressure strength.
 */
struct SetDynamicStrengthCommand {
    double strength;
    static constexpr const char* name() { return "SetDynamicStrengthCommand"; }
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
    // Push-based UI updates
    UIUpdateEvent,

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
    SetDynamicStrengthCommand,
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