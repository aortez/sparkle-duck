#pragma once

#include <variant>
#include <string>
#include <memory>
#include "../Event.h"

namespace DirtSim {
namespace Server {

class StateMachine;

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
    State::Any onEvent(const InitCompleteEvent& evt, StateMachine& dsm);

    static constexpr const char* name() { return "Startup"; }
};

/**
 * @brief Main menu state - user can start simulation or access settings.
 */
struct MainMenu {
    void onEnter(StateMachine& dsm);
    void onExit(StateMachine& dsm);

    State::Any onEvent(const StartSimulationCommand& cmd, StateMachine& dsm);
    State::Any onEvent(const OpenConfigCommand& cmd, StateMachine& dsm);
    State::Any onEvent(const SelectMaterialCommand& cmd, StateMachine& dsm);

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

    void onEnter(StateMachine& dsm);
    void onExit(StateMachine& dsm);

    Any onEvent(const AdvanceSimulationCommand& cmd, StateMachine& dsm);
    Any onEvent(const ApplyScenarioCommand& cmd, StateMachine& dsm);
    Any onEvent(const ResizeWorldCommand& cmd, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::CellGet::Cwc& cwc, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::CellSet::Cwc& cwc, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::GravitySet::Cwc& cwc, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::Reset::Cwc& cwc, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::StateGet::Cwc& cwc, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::StepN::Cwc& cwc, StateMachine& dsm);
    Any onEvent(const PauseCommand& cmd, StateMachine& dsm);
    Any onEvent(const ResetSimulationCommand& cmd, StateMachine& dsm);
    Any onEvent(const SaveWorldCommand& cmd, StateMachine& dsm);
    Any onEvent(const StepBackwardCommand& cmd, StateMachine& dsm);
    Any onEvent(const StepForwardCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleTimeReversalCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetWaterCohesionCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetWaterViscosityCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetWaterPressureThresholdCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetWaterBuoyancyCommand& cmd, StateMachine& dsm);
    Any onEvent(const LoadWorldCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetTimestepCommand& cmd, StateMachine& dsm);
    Any onEvent(const MouseDownEvent& evt, StateMachine& dsm);
    Any onEvent(const MouseMoveEvent& evt, StateMachine& dsm);
    Any onEvent(const MouseUpEvent& evt, StateMachine& dsm);
    Any onEvent(const SelectMaterialCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetTimescaleCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetElasticityCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetDynamicStrengthCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetGravityCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetPressureScaleCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetPressureScaleWorldBCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetCohesionForceStrengthCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetAdhesionStrengthCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetViscosityStrengthCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetFrictionStrengthCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetContactFrictionStrengthCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetCOMCohesionRangeCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetAirResistanceCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleHydrostaticPressureCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleDynamicPressureCommand& cmd, StateMachine& dsm);
    Any onEvent(const TogglePressureDiffusionCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetHydrostaticPressureStrengthCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetDynamicPressureStrengthCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetRainRateCommand& cmd, StateMachine& dsm);

    // Handle immediate events routed through push system
    Any onEvent(const GetFPSCommand& cmd, StateMachine& dsm);
    Any onEvent(const GetSimStatsCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleDebugCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleCohesionForceCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleTimeHistoryCommand& cmd, StateMachine& dsm);
    Any onEvent(const PrintAsciiDiagramCommand& cmd, StateMachine& dsm);
    Any onEvent(const SpawnDirtBallCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetFragmentationCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetPressureSystemCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleWallsCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleWaterColumnCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleLeftThrowCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleRightThrowCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleQuadrantCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleFrameLimitCommand& cmd, StateMachine& dsm);
    Any onEvent(const QuitApplicationCommand& cmd, StateMachine& dsm);

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

    void onEnter(StateMachine& dsm);
    void onExit(StateMachine& dsm);

    Any onEvent(const ResumeCommand& cmd, StateMachine& dsm);
    Any onEvent(const ResetSimulationCommand& cmd, StateMachine& dsm);
    Any onEvent(const AdvanceSimulationCommand& cmd, StateMachine& dsm);
    Any onEvent(const MouseDownEvent& evt, StateMachine& dsm);
    Any onEvent(const MouseMoveEvent& evt, StateMachine& dsm);
    Any onEvent(const MouseUpEvent& evt, StateMachine& dsm);
    Any onEvent(const SelectMaterialCommand& cmd, StateMachine& dsm);

    // Handle immediate events routed through push system
    Any onEvent(const GetFPSCommand& cmd, StateMachine& dsm);
    Any onEvent(const GetSimStatsCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleDebugCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleCohesionForceCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleTimeHistoryCommand& cmd, StateMachine& dsm);
    Any onEvent(const PrintAsciiDiagramCommand& cmd, StateMachine& dsm);
    Any onEvent(const SpawnDirtBallCommand& cmd, StateMachine& dsm);
    Any onEvent(const QuitApplicationCommand& cmd, StateMachine& dsm);

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
    void onEnter(StateMachine& dsm);
    void onExit(StateMachine& dsm);

    Any onEvent(const StartSimulationCommand& cmd, StateMachine& dsm);

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
    void onEnter(StateMachine& dsm);

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
} // namespace Server
} // namespace DirtSim