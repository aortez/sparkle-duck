#include "core/World.h"
#include "server/StateMachine.h"
#include "server/states/Idle.h"
#include "server/states/Shutdown.h"
#include "server/states/SimRunning.h"
#include "server/states/State.h"
#include <gtest/gtest.h>

using namespace DirtSim;
using namespace DirtSim::Server;
using namespace DirtSim::Server::State;

/**
 * @brief Test fixture for Idle state tests.
 *
 * Provides common setup: a StateMachine instance for state context.
 */
class StateIdleTest : public ::testing::Test {
protected:
    void SetUp() override { stateMachine = std::make_unique<StateMachine>(); }

    void TearDown() override { stateMachine.reset(); }

    std::unique_ptr<StateMachine> stateMachine;
};

/**
 * @brief Test that SimRun command creates a World and transitions to SimRunning.
 */
TEST_F(StateIdleTest, SimRunCreatesWorldAndTransitionsToSimRunning)
{
    // Setup: Create Idle state.
    Idle idleState;

    // Setup: Create SimRun command with callback to capture response.
    bool callbackInvoked = false;
    Api::SimRun::Response capturedResponse;

    Api::SimRun::Command cmd;
    cmd.timestep = 0.016; // 60 FPS.
    cmd.max_steps = 100;

    Api::SimRun::Cwc cwc(cmd, [&](Api::SimRun::Response&& response) {
        callbackInvoked = true;
        capturedResponse = std::move(response);
    });

    // Execute: Send SimRun command to Idle state.
    State::Any newState = idleState.onEvent(cwc, *stateMachine);

    // Verify: State transitioned to SimRunning.
    ASSERT_TRUE(std::holds_alternative<SimRunning>(newState.getVariant()))
        << "Idle + SimRun should transition to SimRunning";

    // Verify: SimRunning has valid World.
    SimRunning& simRunning = std::get<SimRunning>(newState.getVariant());
    ASSERT_NE(simRunning.world, nullptr) << "SimRunning should have a World";
    EXPECT_EQ(simRunning.world->getData().width, stateMachine->defaultWidth);
    EXPECT_EQ(simRunning.world->getData().height, stateMachine->defaultHeight);

    // Verify: SimRunning has correct run parameters.
    EXPECT_EQ(simRunning.stepCount, 0u) << "Initial step count should be 0";
    EXPECT_EQ(simRunning.targetSteps, 100u) << "Target steps should match command";
    EXPECT_DOUBLE_EQ(simRunning.stepDurationMs, 16.0) << "Step duration should be 16ms";

    // Note: Scenario application and wall setup happen in SimRunning::onEnter(),
    // which should be tested in StateSimRunning_test.cpp.

    // Verify: Response callback was invoked.
    ASSERT_TRUE(callbackInvoked) << "Response callback should be invoked";
    ASSERT_TRUE(capturedResponse.isValue()) << "Response should be success";
    EXPECT_TRUE(capturedResponse.value().running) << "Response should indicate running";
    EXPECT_EQ(capturedResponse.value().current_step, 0u) << "Initial step number is 0";
}

/**
 * @brief Test that Exit command transitions to Shutdown.
 */
TEST_F(StateIdleTest, ExitCommandTransitionsToShutdown)
{
    // Setup: Create Idle state.
    Idle idleState;

    // Setup: Create Exit command with callback to capture response.
    bool callbackInvoked = false;
    Api::Exit::Response capturedResponse;

    Api::Exit::Command cmd;
    Api::Exit::Cwc cwc(cmd, [&](Api::Exit::Response&& response) {
        callbackInvoked = true;
        capturedResponse = std::move(response);
    });

    // Execute: Send Exit command to Idle state.
    State::Any newState = idleState.onEvent(cwc, *stateMachine);

    // Verify: State transitioned to Shutdown.
    ASSERT_TRUE(std::holds_alternative<Shutdown>(newState.getVariant()))
        << "Idle + Exit should transition to Shutdown";

    // Verify: Response callback was invoked.
    ASSERT_TRUE(callbackInvoked) << "Response callback should be invoked";
    ASSERT_TRUE(capturedResponse.isValue()) << "Response should be success";
}
