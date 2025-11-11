#pragma once

#include "StateForward.h"
#include "server/Event.h"
#include <chrono>
#include <memory>

namespace DirtSim {

class World;

namespace Server {
namespace State {

/**
 * @brief Active simulation state - owns World, physics advancing.
 */
struct SimRunning {
    std::unique_ptr<World> world;
    uint32_t stepCount = 0;
    uint32_t targetSteps = 0;     // Steps to execute before pausing.
    double stepDurationMs = 16.0; // Physics timestep in milliseconds.
    int frameLimit = -1;          // Optional FPS cap (-1 = unlimited).

    // FPS tracking.
    std::chrono::steady_clock::time_point lastFrameTime;
    double actualFPS = 0.0; // Measured FPS (steps/second).

    // Fixed timestep accumulator for deterministic physics.
    double physicsAccumulatorSeconds = 0.0;                 // Accumulated real time.
    static constexpr double FIXED_TIMESTEP_SECONDS = 0.016; // 16ms = 60 FPS physics.
    std::chrono::steady_clock::time_point lastPhysicsTime;

    // UI frame delivery backpressure.
    bool uiReadyForNextFrame = true; // Start true to send first frame.

    void onEnter(StateMachine& dsm);
    void onExit(StateMachine& dsm);

    Any onEvent(const AdvanceSimulationCommand& cmd, StateMachine& dsm);
    Any onEvent(const ApplyScenarioCommand& cmd, StateMachine& dsm);
    Any onEvent(const ResizeWorldCommand& cmd, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::CellGet::Cwc& cwc, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::CellSet::Cwc& cwc, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::DiagramGet::Cwc& cwc, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::Exit::Cwc& cwc, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::FrameReady::Cwc& cwc, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::GravitySet::Cwc& cwc, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::PerfStatsGet::Cwc& cwc, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::PhysicsSettingsGet::Cwc& cwc, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::PhysicsSettingsSet::Cwc& cwc, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::Reset::Cwc& cwc, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::ScenarioConfigSet::Cwc& cwc, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::SeedAdd::Cwc& cwc, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::SimRun::Cwc& cwc, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::SpawnDirtBall::Cwc& cwc, StateMachine& dsm);
    Any onEvent(const DirtSim::Api::StateGet::Cwc& cwc, StateMachine& dsm);
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
    Any onEvent(const GetFPSCommand& cmd, StateMachine& dsm);
    Any onEvent(const GetSimStatsCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleCohesionForceCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleTimeHistoryCommand& cmd, StateMachine& dsm);
    Any onEvent(const PrintAsciiDiagramCommand& cmd, StateMachine& dsm);
    Any onEvent(const SpawnDirtBallCommand& cmd, StateMachine& dsm);
    Any onEvent(const SetFragmentationCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleWallsCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleWaterColumnCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleLeftThrowCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleRightThrowCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleQuadrantCommand& cmd, StateMachine& dsm);
    Any onEvent(const ToggleFrameLimitCommand& cmd, StateMachine& dsm);
    Any onEvent(const QuitApplicationCommand& cmd, StateMachine& dsm);

    static constexpr const char* name() { return "SimRunning"; }
};

} // namespace State
} // namespace Server
} // namespace DirtSim
