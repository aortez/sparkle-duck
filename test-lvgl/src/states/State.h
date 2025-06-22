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
    
    void onEnter(DirtSimStateMachine& dsm);
    void onExit(DirtSimStateMachine& dsm);
    
    Any onEvent(const AdvanceSimulationCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const PauseCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const ResetSimulationCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SaveWorldCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const MouseDownEvent& evt, DirtSimStateMachine& dsm);
    Any onEvent(const SelectMaterialCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetTimescaleCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const SetElasticityCommand& cmd, DirtSimStateMachine& dsm);
    
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
    
    void onEnter(DirtSimStateMachine& dsm);
    void onExit(DirtSimStateMachine& dsm);
    
    Any onEvent(const ResumeCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const ResetSimulationCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const AdvanceSimulationCommand& cmd, DirtSimStateMachine& dsm);
    Any onEvent(const MouseDownEvent& evt, DirtSimStateMachine& dsm);
    Any onEvent(const SelectMaterialCommand& cmd, DirtSimStateMachine& dsm);
    
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