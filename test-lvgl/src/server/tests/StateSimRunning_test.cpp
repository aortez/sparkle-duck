#include "server/states/SimRunning.h"
#include "server/states/Idle.h"
#include "server/states/Shutdown.h"
#include "server/StateMachine.h"
#include "server/scenarios/ScenarioRegistry.h"
#include "core/World.h"
#include "core/Cell.h"
#include "core/ScenarioConfig.h"
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

using namespace DirtSim;
using namespace DirtSim::Server;
using namespace DirtSim::Server::State;

/**
 * @brief Test fixture for SimRunning state tests.
 *
 * Provides common setup including a StateMachine and helper to create initialized SimRunning states.
 */
class StateSimRunningTest : public ::testing::Test {
protected:
    void SetUp() override {
        stateMachine = std::make_unique<StateMachine>();
    }

    void TearDown() override {
        stateMachine.reset();
    }

    /**
     * @brief Helper to create a SimRunning state with initialized world.
     * @return SimRunning state after onEnter() has been called.
     */
    SimRunning createSimRunningWithWorld() {
        // Create Idle and transition to SimRunning.
        Idle idleState;
        Api::SimRun::Command cmd{0.016, 100};
        Api::SimRun::Cwc cwc(cmd, [](auto&&){});
        State::Any state = idleState.onEvent(cwc, *stateMachine);

        SimRunning simRunning = std::move(std::get<SimRunning>(state));

        // Call onEnter to initialize scenario.
        simRunning.onEnter(*stateMachine);

        return simRunning;
    }

    /**
     * @brief Helper to apply clean scenario config (all features disabled).
     */
    void applyCleanScenario(SimRunning& simRunning) {
        SandboxConfig cleanConfig;
        cleanConfig.quadrant_enabled = false;
        cleanConfig.water_column_enabled = false;
        cleanConfig.right_throw_enabled = false;
        cleanConfig.top_drop_enabled = false;
        cleanConfig.rain_rate = 0.0;

        Api::ScenarioConfigSet::Command cmd;
        cmd.config = cleanConfig;
        Api::ScenarioConfigSet::Cwc cwc(cmd, [](auto&&){});

        State::Any newState = simRunning.onEvent(cwc, *stateMachine);
        simRunning = std::move(std::get<SimRunning>(newState));
    }

    std::unique_ptr<StateMachine> stateMachine;
};

/**
 * @brief Test that onEnter applies default sandbox scenario.
 */
TEST_F(StateSimRunningTest, OnEnter_AppliesDefaultScenario)
{
    // Setup: Create SimRunning state (world exists but scenario not applied).
    Idle idleState;
    Api::SimRun::Command cmd{0.016, 100};
    Api::SimRun::Cwc cwc(cmd, [](auto&&){});
    State::Any state = idleState.onEvent(cwc, *stateMachine);
    SimRunning simRunning = std::move(std::get<SimRunning>(state));

    // Verify: World exists but scenario not yet applied.
    ASSERT_NE(simRunning.world, nullptr);
    EXPECT_EQ(simRunning.world->data.scenario_id, "empty") << "Scenario not applied before onEnter";

    // Execute: Call onEnter to apply scenario.
    simRunning.onEnter(*stateMachine);

    // Verify: Sandbox scenario is applied.
    EXPECT_EQ(simRunning.world->data.scenario_id, "sandbox") << "Default scenario should be sandbox";

    // Verify: Walls exist (basic scenario setup check).
    const Cell& topLeft = simRunning.world->at(0, 0);
    const Cell& bottomRight = simRunning.world->at(
        simRunning.world->data.width - 1,
        simRunning.world->data.height - 1);
    EXPECT_EQ(topLeft.material_type, MaterialType::WALL) << "Walls should be created";
    EXPECT_EQ(bottomRight.material_type, MaterialType::WALL) << "Walls should be created";
}

/**
 * @brief Test that AdvanceSimulationCommand steps physics and dirt falls.
 */
TEST_F(StateSimRunningTest, AdvanceSimulation_StepsPhysicsAndDirtFalls)
{
    // Setup: Create initialized SimRunning with clean scenario (no features).
    SimRunning simRunning = createSimRunningWithWorld();
    applyCleanScenario(simRunning);

    // Setup: Manually add dirt at top center.
    const uint32_t testX = 14;
    const uint32_t testY = 5;

    // Debug: Check world state before adding dirt.
    spdlog::info("TEST: World dimensions: {}x{}", simRunning.world->data.width, simRunning.world->data.height);
    spdlog::info("TEST: Gravity: {}", simRunning.world->data.gravity);
    spdlog::info("TEST: Total mass before adding dirt: {}", simRunning.world->getTotalMass());

    simRunning.world->at(testX, testY).addDirt(1.0);

    spdlog::info("TEST: Total mass after adding dirt: {}", simRunning.world->getTotalMass());

    // Verify initial state.
    const Cell& startCell = simRunning.world->at(testX, testY);
    const Cell& cellBelow = simRunning.world->at(testX, testY + 1);
    spdlog::info("TEST: Start cell ({},{}) material={}, fill={}",
                 testX, testY, static_cast<int>(startCell.material_type), startCell.fill_ratio);
    spdlog::info("TEST: Cell below ({},{}) material={}, fill={}",
                 testX, testY + 1, static_cast<int>(cellBelow.material_type), cellBelow.fill_ratio);

    EXPECT_EQ(startCell.material_type, MaterialType::DIRT) << "Should have dirt at starting position";
    EXPECT_GT(startCell.fill_ratio, 0.9) << "Dirt should be nearly full";
    EXPECT_LT(cellBelow.fill_ratio, 0.1) << "Cell below should be empty initially";

    // Execute: Advance simulation up to 200 frames, checking for dirt movement.
    bool dirtFell = false;
    for (int i = 0; i < 200; ++i) {
        State::Any newState = simRunning.onEvent(AdvanceSimulationCommand{}, *stateMachine);
        simRunning = std::move(std::get<SimRunning>(newState));

        // Debug: Log first few steps.
        if (i < 5 || i % 20 == 0) {
            const Cell& current = simRunning.world->at(testX, testY);
            const Cell& below = simRunning.world->at(testX, testY + 1);
            spdlog::info("TEST: Step {} - Cell({},{}) mat={} fill={:.2f} COM=({:.3f},{:.3f}) vel=({:.3f},{:.3f})",
                         i + 1, testX, testY,
                         static_cast<int>(current.material_type), current.fill_ratio,
                         current.com.x, current.com.y,
                         current.velocity.x, current.velocity.y);
            spdlog::info("TEST: Step {} - Cell({},{}) mat={} fill={:.2f}",
                         i + 1, testX, testY + 1,
                         static_cast<int>(below.material_type), below.fill_ratio);
        }

        // Check if dirt has moved to cell below.
        const Cell& cellBelow = simRunning.world->at(testX, testY + 1);
        if (cellBelow.material_type == MaterialType::DIRT && cellBelow.fill_ratio > 0.1) {
            dirtFell = true;
            spdlog::info("Dirt fell after {} steps", i + 1);
            break;
        }
    }

    // Verify: Dirt fell to the cell below within 200 frames.
    ASSERT_TRUE(dirtFell) << "Dirt should fall to next cell within 200 frames";
    const Cell& finalCellBelow = simRunning.world->at(testX, testY + 1);
    EXPECT_EQ(finalCellBelow.material_type, MaterialType::DIRT) << "Cell below should have dirt";
    EXPECT_GT(finalCellBelow.fill_ratio, 0.1) << "Cell below should have dirt";
    EXPECT_GT(simRunning.stepCount, 0u) << "Step count should have increased";
}

/**
 * @brief Test that StateGet returns correct WorldData.
 */
TEST_F(StateSimRunningTest, StateGet_ReturnsWorldData)
{
    // Setup: Create initialized SimRunning.
    SimRunning simRunning = createSimRunningWithWorld();

    // Setup: Create StateGet command with callback to capture response.
    bool callbackInvoked = false;
    Api::StateGet::Response capturedResponse;

    Api::StateGet::Command cmd;
    Api::StateGet::Cwc cwc(cmd, [&](Api::StateGet::Response&& response) {
        callbackInvoked = true;
        capturedResponse = std::move(response);
    });

    // Execute: Send StateGet command.
    State::Any newState = simRunning.onEvent(cwc, *stateMachine);

    // Verify: Stays in SimRunning.
    ASSERT_TRUE(std::holds_alternative<SimRunning>(newState));

    // Verify: Callback was invoked with success.
    ASSERT_TRUE(callbackInvoked) << "StateGet callback should be invoked";
    ASSERT_TRUE(capturedResponse.isValue()) << "StateGet should return success";

    // Verify: WorldData has correct properties.
    const WorldData& worldData = capturedResponse.value().worldData;
    EXPECT_EQ(worldData.width, stateMachine->defaultWidth);
    EXPECT_EQ(worldData.height, stateMachine->defaultHeight);
    EXPECT_EQ(worldData.scenario_id, "sandbox");
    EXPECT_EQ(worldData.timestep, simRunning.stepCount);
}

/**
 * @brief Test that ScenarioConfigSet toggles water column off and on.
 */
TEST_F(StateSimRunningTest, ScenarioConfigSet_TogglesWaterColumn)
{
    // Setup: Create initialized SimRunning (water column ON by default).
    SimRunning simRunning = createSimRunningWithWorld();

    // Verify: Water column initially exists (check a few cells).
    const Cell& waterCell = simRunning.world->at(3, 10);
    EXPECT_EQ(waterCell.material_type, MaterialType::WATER) << "Water column should exist initially";
    EXPECT_GT(waterCell.fill_ratio, 0.5) << "Water column cells should be filled";

    // Execute: Toggle water column OFF.
    SandboxConfig configOff;
    configOff.quadrant_enabled = true;  // Keep quadrant.
    configOff.water_column_enabled = false;  // Turn off water column.
    configOff.right_throw_enabled = false;
    configOff.top_drop_enabled = false;
    configOff.rain_rate = 0.0;

    bool callbackInvoked = false;
    Api::ScenarioConfigSet::Command cmdOff;
    cmdOff.config = configOff;
    Api::ScenarioConfigSet::Cwc cwcOff(cmdOff, [&](auto&& response) {
        callbackInvoked = true;
        EXPECT_TRUE(response.isValue()) << "ScenarioConfigSet should succeed";
    });

    State::Any stateAfterOff = simRunning.onEvent(cwcOff, *stateMachine);
    simRunning = std::move(std::get<SimRunning>(stateAfterOff));

    // Verify: Water column removed.
    ASSERT_TRUE(callbackInvoked) << "Callback should be invoked";
    for (uint32_t y = 0; y < 20; ++y) {
        for (uint32_t x = 1; x <= 5; ++x) {
            const Cell& cell = simRunning.world->at(x, y);
            EXPECT_TRUE(cell.material_type != MaterialType::WATER || cell.fill_ratio < 0.1)
                << "Water column cells should be cleared at (" << x << "," << y << ")";
        }
    }

    // Execute: Toggle water column back ON.
    callbackInvoked = false;
    SandboxConfig configOn = configOff;
    configOn.water_column_enabled = true;

    Api::ScenarioConfigSet::Command cmdOn;
    cmdOn.config = configOn;
    Api::ScenarioConfigSet::Cwc cwcOn(cmdOn, [&](auto&& response) {
        callbackInvoked = true;
        EXPECT_TRUE(response.isValue());
    });

    State::Any stateAfterOn = simRunning.onEvent(cwcOn, *stateMachine);
    simRunning = std::move(std::get<SimRunning>(stateAfterOn));

    // Verify: Water column restored.
    ASSERT_TRUE(callbackInvoked);
    const Cell& restoredWaterCell = simRunning.world->at(3, 10);
    EXPECT_EQ(restoredWaterCell.material_type, MaterialType::WATER) << "Water column should be restored";
    EXPECT_GT(restoredWaterCell.fill_ratio, 0.9) << "Water should be nearly full";
}

/**
 * @brief Test that ScenarioConfigSet toggles dirt quadrant off and on.
 */
TEST_F(StateSimRunningTest, ScenarioConfigSet_TogglesDirtQuadrant)
{
    // Setup: Create initialized SimRunning (quadrant ON by default).
    SimRunning simRunning = createSimRunningWithWorld();

    // Verify: Dirt quadrant initially exists (check a cell in lower-right).
    uint32_t quadX = simRunning.world->data.width - 5;
    uint32_t quadY = simRunning.world->data.height - 5;
    const Cell& quadCell = simRunning.world->at(quadX, quadY);
    EXPECT_EQ(quadCell.material_type, MaterialType::DIRT) << "Quadrant should exist initially";
    EXPECT_GT(quadCell.fill_ratio, 0.5) << "Quadrant cells should be filled";

    // Execute: Toggle quadrant OFF.
    SandboxConfig configOff;
    configOff.quadrant_enabled = false;  // Turn off quadrant.
    configOff.water_column_enabled = false;
    configOff.right_throw_enabled = false;
    configOff.top_drop_enabled = false;
    configOff.rain_rate = 0.0;

    bool callbackInvoked = false;
    Api::ScenarioConfigSet::Command cmdOff;
    cmdOff.config = configOff;
    Api::ScenarioConfigSet::Cwc cwcOff(cmdOff, [&](auto&& response) {
        callbackInvoked = true;
        EXPECT_TRUE(response.isValue());
    });

    State::Any stateAfterOff = simRunning.onEvent(cwcOff, *stateMachine);
    simRunning = std::move(std::get<SimRunning>(stateAfterOff));

    // Verify: Quadrant removed.
    ASSERT_TRUE(callbackInvoked);
    const Cell& clearedCell = simRunning.world->at(quadX, quadY);
    EXPECT_TRUE(clearedCell.material_type != MaterialType::DIRT || clearedCell.fill_ratio < 0.1)
        << "Quadrant should be cleared";

    // Execute: Toggle quadrant back ON.
    callbackInvoked = false;
    SandboxConfig configOn = configOff;
    configOn.quadrant_enabled = true;

    Api::ScenarioConfigSet::Command cmdOn;
    cmdOn.config = configOn;
    Api::ScenarioConfigSet::Cwc cwcOn(cmdOn, [&](auto&& response) {
        callbackInvoked = true;
        EXPECT_TRUE(response.isValue());
    });

    State::Any stateAfterOn = simRunning.onEvent(cwcOn, *stateMachine);
    simRunning = std::move(std::get<SimRunning>(stateAfterOn));

    // Verify: Quadrant restored.
    ASSERT_TRUE(callbackInvoked);
    const Cell& restoredCell = simRunning.world->at(quadX, quadY);
    EXPECT_EQ(restoredCell.material_type, MaterialType::DIRT) << "Quadrant should be restored";
    EXPECT_GT(restoredCell.fill_ratio, 0.9) << "Quadrant cells should be filled";
}

/**
 * @brief Test that Exit command transitions to Shutdown.
 */
TEST_F(StateSimRunningTest, Exit_TransitionsToShutdown)
{
    // Setup: Create initialized SimRunning.
    SimRunning simRunning = createSimRunningWithWorld();

    // Setup: Create Exit command with callback.
    bool callbackInvoked = false;
    Api::Exit::Command cmd;
    Api::Exit::Cwc cwc(cmd, [&](Api::Exit::Response&& response) {
        callbackInvoked = true;
        EXPECT_TRUE(response.isValue());
    });

    // Execute: Send Exit command.
    State::Any newState = simRunning.onEvent(cwc, *stateMachine);

    // Verify: Transitioned to Shutdown.
    ASSERT_TRUE(std::holds_alternative<Shutdown>(newState)) << "Exit should transition to Shutdown";
    ASSERT_TRUE(callbackInvoked) << "Exit callback should be invoked";
}

/**
 * @brief Test that SimRun updates run parameters without recreating world.
 */
TEST_F(StateSimRunningTest, SimRun_UpdatesRunParameters)
{
    // Setup: Create initialized SimRunning with initial parameters.
    SimRunning simRunning = createSimRunningWithWorld();
    EXPECT_EQ(simRunning.targetSteps, 100u);
    EXPECT_DOUBLE_EQ(simRunning.stepDurationMs, 16.0);

    // Advance a few steps to verify world isn't recreated.
    for (int i = 0; i < 5; ++i) {
        State::Any state = simRunning.onEvent(AdvanceSimulationCommand{}, *stateMachine);
        simRunning = std::move(std::get<SimRunning>(state));
    }
    EXPECT_EQ(simRunning.stepCount, 5u);

    // Execute: Send SimRun with new parameters.
    bool callbackInvoked = false;
    Api::SimRun::Command cmd{0.032, 50};  // Different timestep and target.
    Api::SimRun::Cwc cwc(cmd, [&](Api::SimRun::Response&& response) {
        callbackInvoked = true;
        EXPECT_TRUE(response.isValue());
    });

    State::Any newState = simRunning.onEvent(cwc, *stateMachine);
    simRunning = std::move(std::get<SimRunning>(newState));

    // Verify: Parameters updated but world preserved.
    ASSERT_TRUE(callbackInvoked);
    EXPECT_EQ(simRunning.targetSteps, 50u) << "Target steps should be updated";
    EXPECT_DOUBLE_EQ(simRunning.stepDurationMs, 32.0) << "Step duration should be updated";
    EXPECT_EQ(simRunning.stepCount, 5u) << "Step count should be preserved (world not recreated)";
    EXPECT_TRUE(std::holds_alternative<SimRunning>(newState)) << "Should stay in SimRunning";
}
