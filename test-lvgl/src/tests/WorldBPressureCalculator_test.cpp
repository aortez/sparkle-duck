#include <gtest/gtest.h>
#include "../WorldB.h"
#include "../WorldBPressureCalculator.h"
#include "../MaterialType.h"
#include <spdlog/spdlog.h>

class WorldBPressureCalculatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a 6x6 world.
        world = std::make_unique<WorldB>(6, 6);
        
        // Enable dynamic pressure for these tests.
        world->setPressureSystem(WorldInterface::PressureSystem::TopDown);
        world->setDynamicPressureEnabled(true);
        world->setHydrostaticPressureEnabled(false);
        world->setPressureScale(1.0);
        
        // Get reference to pressure calculator.
        pressureCalc = &world->getPressureCalculator();
    }
    
    std::unique_ptr<WorldB> world;
    WorldBPressureCalculator* pressureCalc;
};

// 3. Processing Logic Tests.

// Data-driven test structure for blocked transfers to different materials.
struct BlockedTransferTestCase {
    std::string name;
    MaterialType targetMaterial;
    double expectedPressureChange;  // 0 for walls, >0 for other materials.
    std::string expectedBehavior;
};

class ProcessBlockedTransfersTest : public WorldBPressureCalculatorTest,
                                   public ::testing::WithParamInterface<BlockedTransferTestCase> {
};

TEST_P(ProcessBlockedTransfersTest, ProcessBlockedTransfers_HandlesTargetMaterialCorrectly) {
    const auto& testCase = GetParam();
    
    // Setup: Create target cell with specified material.
    const int targetX = 2, targetY = 2;
    world->addMaterialAtCell(targetX, targetY, testCase.targetMaterial, 1.0);
    CellB& targetCell = world->at(targetX, targetY);
    
    // Record initial pressure (should be 0).
    double initialPressure = targetCell.getDynamicPressure();
    EXPECT_EQ(0.0, initialPressure) << "Initial pressure should be zero";
    
    // Create a blocked transfer TO the target cell.
    WorldBPressureCalculator::BlockedTransfer transfer{};
    transfer.fromX = 1;
    transfer.fromY = 2;
    transfer.toX = targetX;
    transfer.toY = targetY;
    transfer.transfer_amount = 0.5;
    transfer.velocity = Vector2d(2.0, 0.0);  // Rightward velocity.
    transfer.energy = 2.0;  // kinetic energy = 0.5 * mass * velocity^2.
    
    // Queue and process the transfer.
    pressureCalc->queueBlockedTransfer(transfer);
    pressureCalc->processBlockedTransfers(pressureCalc->blocked_transfers_);
    
    // Get final pressure.
    double finalPressure = targetCell.getDynamicPressure();
    
    // Verify expected behavior based on material type.
    if (testCase.expectedPressureChange == 0.0) {
        EXPECT_EQ(initialPressure, finalPressure) 
            << "Material " << getMaterialName(testCase.targetMaterial) 
            << " should not accumulate pressure: " << testCase.expectedBehavior;
    } else {
        EXPECT_GT(finalPressure, initialPressure) 
            << "Material " << getMaterialName(testCase.targetMaterial) 
            << " should accumulate pressure from blocked transfers";
        
        // For materials that accumulate pressure, verify it's proportional to energy and material weight.
        double materialWeight = pressureCalc->getDynamicWeight(testCase.targetMaterial);
        double expectedPressure = transfer.energy * materialWeight;
        EXPECT_NEAR(finalPressure, expectedPressure, 0.001)
            << "Pressure should equal energy * material_weight for " 
            << getMaterialName(testCase.targetMaterial);
    }
    
    // Additional verification for pressure (if pressure was added).
    if (finalPressure > 0.0) {
        // Verify pressure is positive.
        EXPECT_GT(targetCell.getHydrostaticPressure(), 0.0) 
            << "Pressure should be positive when added";
    }
}

// Test cases covering different target materials.
INSTANTIATE_TEST_SUITE_P(
    TargetMaterialVariations,
    ProcessBlockedTransfersTest,
    ::testing::Values(
        BlockedTransferTestCase{
            "TransferToWall",
            MaterialType::WALL,
            0.0,  // No pressure accumulation.
            "Walls eliminate pressure completely"
        },
        BlockedTransferTestCase{
            "TransferToMetal",
            MaterialType::METAL,
            0.5,  // METAL has dynamic weight of 0.5.
            "Metal cells should accumulate reduced pressure"
        },
        BlockedTransferTestCase{
            "TransferToWater",
            MaterialType::WATER,
            0.8,  // WATER has dynamic weight of 0.8.
            "Water cells should accumulate high pressure"
        },
        BlockedTransferTestCase{
            "TransferToDirt",
            MaterialType::DIRT,
            1.0,  // DIRT has dynamic weight of 1.0.
            "Dirt cells should accumulate full pressure"
        }
    ),
    [](const ::testing::TestParamInfo<BlockedTransferTestCase>& info) {
        return info.param.name;
    }
);

TEST_F(WorldBPressureCalculatorTest, DISABLED_ProcessBlockedTransfers_IgnoresTransfersToEmptyCells) {
    // Queue transfer to empty (AIR) cell.
    // Process and verify no pressure accumulation.
    
    // TODO: Enable this test once implemented.
    FAIL() << "Test not yet implemented";
}

TEST_F(WorldBPressureCalculatorTest, DISABLED_ProcessBlockedTransfers_AccumulatesPressureInNonEmptyTargets) {
    // Queue transfer to WATER/DIRT/etc cell.
    // Verify pressure increases by expected amount.
    
    // TODO: Enable this test once implemented.
    FAIL() << "Test not yet implemented";
}

// 4. Material Weight Tests.

TEST_F(WorldBPressureCalculatorTest, DISABLED_ProcessBlockedTransfers_AppliesMaterialSpecificWeights) {
    // Queue same energy transfer to different materials.
    // Verify DIRT (weight=1.0) gets more pressure than METAL (weight=0.5).
    
    // TODO: Enable this test once implemented.
    FAIL() << "Test not yet implemented";
}

// 5. Pressure Vector Tests.

TEST_F(WorldBPressureCalculatorTest, DISABLED_ProcessBlockedTransfers_UpdatesPressureVector) {
    // Queue transfer with specific velocity direction.
    // Verify pressure vector aligns with blocked velocity.
    
    // TODO: Enable this test once implemented.
    FAIL() << "Test not yet implemented";
}

TEST_F(WorldBPressureCalculatorTest, DISABLED_ProcessBlockedTransfers_CombinesPressureVectors) {
    // Multiple transfers to same cell.
    // Verify weighted average of pressure vectors.
    
    // TODO: Enable this test once implemented.
    FAIL() << "Test not yet implemented";
}

// 6. Edge Cases.

TEST_F(WorldBPressureCalculatorTest, DISABLED_BlockedTransfers_ZeroEnergyTransfer) {
    // Transfer with zero velocity or amount.
    // Should not create pressure.
    
    // TODO: Enable this test once implemented.
    FAIL() << "Test not yet implemented";
}

TEST_F(WorldBPressureCalculatorTest, DISABLED_BlockedTransfers_MaxPressureLimit) {
    // Queue many high-energy transfers.
    // Does pressure have a cap? Should it?
    
    // TODO: Enable this test once implemented.
    FAIL() << "Test not yet implemented";
}

TEST_F(WorldBPressureCalculatorTest, DISABLED_BlockedTransfers_SimultaneousTransfersToSameCell) {
    // Multiple sources transferring to same target.
    // All should accumulate.
    
    // TODO: Enable this test once implemented.
    FAIL() << "Test not yet implemented";
}
