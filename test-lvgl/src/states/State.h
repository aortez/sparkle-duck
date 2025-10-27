#pragma once

#include <variant>
#include <string>
#include <memory>
#include "../Event.h"

namespace DirtSim {

// Forward declarations
class DirtSimStateMachine;

namespace State {

// Forward declare the variant type
struct Startup;
struct MainMenu;
struct SimRunning;
struct SimPaused;
struct UnitTesting;
struct Benchmarking;
struct Loading;
struct Saving;
struct Config;
struct Demo;
struct Shutdown;

using Any = std::variant<
    Startup,
    MainMenu,
    SimRunning,
    SimPaused,
    UnitTesting,
    Benchmarking,
    Loading,
    Saving,
    Config,
    Demo,
    Shutdown
>;

/**
 * @brief Initial startup state - loading resources and initializing systems.
 */
struct Startup {
    State::Any onEvent(const InitCompleteEvent& evt, DirtSimStateMachine& dsm);
    
    static constexpr const char* name() { return "Startup"; }
};

/**
 * @brief Main menu state - user can start simulation or access settings.
 */
struct MainMenu {
    void onEnter(DirtSimStateMachine& dsm);
    void onExit(DirtSimStateMachine& dsm);
    
    State::Any onEvent(const StartSimulationCommand& cmd, DirtSimStateMachine& dsm);
    State::Any onEvent(const OpenConfigCommand& cmd, DirtSimStateMachine& dsm);
    State::Any onEvent(const SelectMaterialCommand& cmd, DirtSimStateMachine& dsm);
    
    static constexpr const char* name() { return "MainMenu"; }
};

/**
 * @brief Active simulation state - physics running and UI interactive.
 */
struct SimRunning {
    uint32_t stepCount = 0;

    // Interaction mode for smart cell grabber.
    enum class InteractionMode {
        NONE,       // No interaction active.
        GRAB_MODE   // Dragging material (either existing or newly created).
    };
    InteractionMode interactionMode = InteractionMode::NONE;

    void onEnter(DirtSimStateMachine& dsm);
    void onExit(DirtSimStateMachine& dsm);

    Any onEvent(const AdvanceSimulationCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const PauseCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const ResetSimulationCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SaveWorldCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const StepBackwardCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const StepForwardCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const ToggleTimeReversalCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetWaterCohesionCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetWaterViscosityCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetWaterPressureThresholdCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetWaterBuoyancyCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const LoadWorldCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetTimestepCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const MouseDownEvent& evt, DirtSimStateMachine& dsm);
    Any onEvent(const MouseMoveEvent& evt, DirtSimStateMachine& dsm);
    Any onEvent(const MouseUpEvent& evt, DirtSimStateMachine& dsm);
    Any onEvent(const SelectMaterialCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetTimescaleCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetElasticityCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetDynamicStrengthCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetGravityCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetPressureScaleCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetPressureScaleWorldBCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetCohesionForceStrengthCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetAdhesionStrengthCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetViscosityStrengthCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetFrictionStrengthCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetCOMCohesionRangeCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetAirResistanceCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const ToggleHydrostaticPressureCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const ToggleDynamicPressureCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const TogglePressureDiffusionCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetHydrostaticPressureStrengthCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetDynamicPressureStrengthCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetRainRateCommand& cmd, DirtSimStateMachine& dsm);

    // Handle immediate events routed through push system
    Any onEvent(const GetFPSCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const GetSimStatsCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const ToggleDebugCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const ToggleCohesionForceCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const ToggleTimeHistoryCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const PrintAsciiDiagramCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetFragmentationCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetPressureSystemCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const ToggleWallsCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const ToggleWaterColumnCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const ToggleLeftThrowCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const ToggleRightThrowCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const ToggleQuadrantCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const ToggleFrameLimitCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const QuitApplicationCommand& cmd, DirtSimStateMachine& dsm);

    static constexpr const char* name() { return "SimRunning"; }
};

/**
 * @brief Paused simulation state - physics halted but UI remains active.
 * 
 * For now, only supports pausing from SimRunning. Can be extended later
 * to support pausing from other states.
 */
struct SimPaused {
    // Store the previous SimRunning state with all its data
    SimRunning previousState;
    // Store the timescale before pausing so we can restore it
    double previousTimescale = 1.0;

    void onEnter(DirtSimStateMachine& dsm);
    void onExit(DirtSimStateMachine& dsm);

    Any onEvent(const ResumeCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const ResetSimulationCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const AdvanceSimulationCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const MouseDownEvent& evt, DirtSimStateMachine& dsm);
    Any onEvent(const MouseMoveEvent& evt, DirtSimStateMachine& dsm);
    Any onEvent(const MouseUpEvent& evt, DirtSimStateMachine& dsm);
    Any onEvent(const SelectMaterialCommand& cmd, DirtSimStateMachine& dsm);
    
    // Handle immediate events routed through push system
    Any onEvent(const GetFPSCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const GetSimStatsCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const ToggleDebugCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const ToggleCohesionForceCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const ToggleTimeHistoryCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const PrintAsciiDiagramCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const QuitApplicationCommand& cmd, DirtSimStateMachine& dsm);

    static constexpr const char* name() { return "SimPaused"; }
};

/**
 * @brief Unit testing state - running automated tests.
 */
struct UnitTesting {
    std::string currentTest;
    static constexpr const char* name() { return "UnitTesting"; }
};

/**
 * @brief Performance benchmarking state.
 */
struct Benchmarking {
    uint32_t iterationsRemaining = 0;
    static constexpr const char* name() { return "Benchmarking"; }
};

/**
 * @brief Loading saved simulation state.
 */
struct Loading {
    std::string filepath;
    static constexpr const char* name() { return "Loading"; }
};

/**
 * @brief Saving current simulation state.
 */
struct Saving {
    std::string filepath;
    static constexpr const char* name() { return "Saving"; }
};

/**
 * @brief Configuration/settings state.
 */
struct Config {
    void onEnter(DirtSimStateMachine& dsm);
    void onExit(DirtSimStateMachine& dsm);
    
    Any onEvent(const StartSimulationCommand& cmd, DirtSimStateMachine& dsm);
    
    static constexpr const char* name() { return "Config"; }
};

/**
 * @brief Demo/tutorial mode state.
 */
struct Demo {
    uint32_t demoStep = 0;
    static constexpr const char* name() { return "Demo"; }
};

/**
 * @brief Shutdown state - cleanup and exit.
 */
struct Shutdown {
    void onEnter(DirtSimStateMachine& dsm);
    
    static constexpr const char* name() { return "Shutdown"; }
};

// Variant is already declared at the top

/**
 * @brief Get the name of the current state.
 */
inline std::string getCurrentStateName(const Any& state) {
    return std::visit([](const auto& s) { return std::string(s.name()); }, state);
}

} // namespace State
} // namespace DirtSim