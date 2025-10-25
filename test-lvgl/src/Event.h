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
 * Transport struct for UIUpdateEvent (not source of truth).
 */
struct PhysicsParams {
    double gravity = 9.81;
    double elasticity = 0.8;
    double timescale = 1.0;
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
 * @brief Set water cohesion parameter for WorldA/RulesA.
 */
struct SetWaterCohesionCommand {
    double cohesion_value;
    static constexpr const char* name() { return "SetWaterCohesionCommand"; }
};

/**
 * @brief Set water viscosity parameter for WorldA/RulesA.
 */
struct SetWaterViscosityCommand {
    double viscosity_value;
    static constexpr const char* name() { return "SetWaterViscosityCommand"; }
};

/**
 * @brief Set water pressure threshold parameter for WorldA/RulesA.
 */
struct SetWaterPressureThresholdCommand {
    double threshold_value;
    static constexpr const char* name() { return "SetWaterPressureThresholdCommand"; }
};

/**
 * @brief Set water buoyancy parameter for WorldA/RulesA.
 */
struct SetWaterBuoyancyCommand {
    double buoyancy_value;
    static constexpr const char* name() { return "SetWaterBuoyancyCommand"; }
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
 * @brief Set WorldB pressure scale factor.
 */
struct SetPressureScaleWorldBCommand {
    double scale;
    static constexpr const char* name() { return "SetPressureScaleWorldBCommand"; }
};

/**
 * @brief Set cohesion force strength.
 */
struct SetCohesionForceStrengthCommand {
    double strength;
    static constexpr const char* name() { return "SetCohesionForceStrengthCommand"; }
};

/**
 * @brief Set adhesion strength.
 */
struct SetAdhesionStrengthCommand {
    double strength;
    static constexpr const char* name() { return "SetAdhesionStrengthCommand"; }
};

/**
 * @brief Set viscosity strength factor.
 */
struct SetViscosityStrengthCommand {
    double strength;
    static constexpr const char* name() { return "SetViscosityStrengthCommand"; }
};

/**
 * @brief Set friction strength factor.
 */
struct SetFrictionStrengthCommand {
    double strength;
    static constexpr const char* name() { return "SetFrictionStrengthCommand"; }
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
 * @brief Toggle hydrostatic pressure system.
 */
struct ToggleHydrostaticPressureCommand {
    static constexpr const char* name() { return "ToggleHydrostaticPressureCommand"; }
};

/**
 * @brief Toggle dynamic pressure system.
 */
struct ToggleDynamicPressureCommand {
    static constexpr const char* name() { return "ToggleDynamicPressureCommand"; }
};

/**
 * @brief Toggle pressure diffusion system.
 */
struct TogglePressureDiffusionCommand {
    static constexpr const char* name() { return "TogglePressureDiffusionCommand"; }
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
 * @brief Toggle cohesion force physics.
 */
struct ToggleCohesionForceCommand {
    static constexpr const char* name() { return "ToggleCohesionForceCommand"; }
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
// MATERIAL & WORLD CONTROLS
// =================================================================

/**
 * @brief Set cell size for display/interaction.
 */
struct SetCellSizeCommand {
    double size;
    static constexpr const char* name() { return "SetCellSizeCommand"; }
};

/**
 * @brief Set fragmentation factor for material breaking.
 */
struct SetFragmentationCommand {
    double factor;
    static constexpr const char* name() { return "SetFragmentationCommand"; }
};

/**
 * @brief Set pressure system type.
 */
struct SetPressureSystemCommand {
    WorldInterface::PressureSystem system;
    static constexpr const char* name() { return "SetPressureSystemCommand"; }
};

/**
 * @brief Toggle wall boundaries on/off.
 */
struct ToggleWallsCommand {
    static constexpr const char* name() { return "ToggleWallsCommand"; }
};

/**
 * @brief Toggle left throw mode.
 */
struct ToggleLeftThrowCommand {
    static constexpr const char* name() { return "ToggleLeftThrowCommand"; }
};

/**
 * @brief Toggle right throw mode.
 */
struct ToggleRightThrowCommand {
    static constexpr const char* name() { return "ToggleRightThrowCommand"; }
};

/**
 * @brief Toggle quadrant selection mode.
 */
struct ToggleQuadrantCommand {
    static constexpr const char* name() { return "ToggleQuadrantCommand"; }
};

/**
 * @brief Toggle frame rate limiting.
 */
struct ToggleFrameLimitCommand {
    static constexpr const char* name() { return "ToggleFrameLimitCommand"; }
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
    LoadWorldCommand,
    StepBackwardCommand,
    StepForwardCommand,
    ToggleTimeReversalCommand,
    SetWaterCohesionCommand,
    SetWaterViscosityCommand,
    SetWaterPressureThresholdCommand,
    SetWaterBuoyancyCommand,
    SetTimestepCommand,

    // Mouse events
    MouseDownEvent,
    MouseMoveEvent,
    MouseUpEvent,

    // Physics parameters
    SetGravityCommand,
    SetElasticityCommand,
    SetTimescaleCommand,
    SetDynamicStrengthCommand,
    SetPressureScaleCommand,
    SetPressureScaleWorldBCommand,
    SetCohesionForceStrengthCommand,
    SetAdhesionStrengthCommand,
    SetViscosityStrengthCommand,
    SetFrictionStrengthCommand,
    SetCOMCohesionRangeCommand,
    SetAirResistanceCommand,
    ToggleHydrostaticPressureCommand,
    ToggleDynamicPressureCommand,
    TogglePressureDiffusionCommand,
    SetHydrostaticPressureStrengthCommand,
    SetDynamicPressureStrengthCommand,
    SetRainRateCommand,
    ToggleDebugCommand,
    ToggleForceCommand,
    ToggleCohesionCommand,
    ToggleCohesionForceCommand,
    ToggleAdhesionCommand,
    ToggleTimeHistoryCommand,

    // Material & world controls
    SetCellSizeCommand,
    SetFragmentationCommand,
    SetPressureSystemCommand,
    ToggleWallsCommand,
    ToggleLeftThrowCommand,
    ToggleRightThrowCommand,
    ToggleQuadrantCommand,
    ToggleFrameLimitCommand,

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