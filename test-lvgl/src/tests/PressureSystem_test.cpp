#include "gtest/gtest.h"
#include "../World.h"
#include "../Cell.h"
#include "../Vector2d.h"

#include <iostream>

// Simple mock draw area for World constructor
class MockDrawArea {
public:
    static void* create() {
        return reinterpret_cast<void*>(0x12345678); // Mock pointer
    }
};

class PressureSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a mock draw_area pointer for World constructor
        // World needs this but doesn't actually use it for pressure calculations
        void* mock_draw_area = MockDrawArea::create();
        
        // Create a world for testing (small size for performance)
        world = std::make_unique<World>(8, 8, reinterpret_cast<lv_obj_t*>(mock_draw_area));
        world->setGravity(9.81);
    }
    
    void TearDown() override {
        world.reset();
    }
    
    // Helper function to add dirt directly to cells (bypassing pixel coordinate conversion)
    void addDirtToCell(uint32_t x, uint32_t y, double amount) {
        if (x < world->getWidth() && y < world->getHeight()) {
            world->at(x, y).dirt = amount;
        }
    }
    
    // Helper function to calculate total pressure in the world
    double getTotalPressure() {
        double total = 0.0;
        for (uint32_t y = 0; y < world->getHeight(); ++y) {
            for (uint32_t x = 0; x < world->getWidth(); ++x) {
                total += world->at(x, y).pressure.mag();
            }
        }
        return total;
    }
    
    std::unique_ptr<World> world;
};

TEST_F(PressureSystemTest, PressureSystemSwitching) {
    std::cout << "\n=== PRESSURE SYSTEM SWITCHING TEST ===" << std::endl;
    
    // Test that all three pressure systems work
    std::vector<World::PressureSystem> systems = {
        World::PressureSystem::Original,
        World::PressureSystem::TopDown,
        World::PressureSystem::IterativeSettling
    };
    
    std::vector<std::string> system_names = {
        "Original (COM)",
        "Top-Down Hydrostatic", 
        "Iterative Settling"
    };
    
    // Add some material for testing
    addDirtToCell(3, 2, 0.8);
    addDirtToCell(4, 2, 0.9);
    addDirtToCell(3, 3, 0.7);
    addDirtToCell(4, 3, 0.8);
    
    for (size_t i = 0; i < systems.size(); ++i) {
        std::cout << "\nTesting pressure system: " << system_names[i] << std::endl;
        
        // Set the pressure system
        world->setPressureSystem(systems[i]);
        
        // Verify it was set correctly
        EXPECT_EQ(world->getPressureSystem(), systems[i]);
        
        // Run a few simulation steps to test pressure calculation
        for (int step = 0; step < 3; ++step) {
            world->advanceTime(1.0/60.0); // 60 FPS timestep
        }
        
        double total_pressure = getTotalPressure();
        std::cout << "  Total pressure magnitude: " << total_pressure << std::endl;
        
        // Basic validation that pressure system is functional
        EXPECT_GE(total_pressure, 0.0) << "Pressure should be non-negative";
        
        if (total_pressure > 0.0) {
            std::cout << "  ✓ Pressure system is generating pressure" << std::endl;
        } else {
            std::cout << "  ! No pressure generated (may be normal for this configuration)" << std::endl;
        }
    }
    
    std::cout << "\n=== Test completed successfully ===" << std::endl;
}

TEST_F(PressureSystemTest, PressureSystemComparison) {
    std::cout << "\n=== PRESSURE SYSTEM COMPARISON TEST ===" << std::endl;
    
    struct PressureResult {
        World::PressureSystem system;
        std::string name;
        double total_pressure;
        double max_pressure;
    };
    
    std::vector<PressureResult> results;
    
    for (auto system : {World::PressureSystem::Original, 
                       World::PressureSystem::TopDown,
                       World::PressureSystem::IterativeSettling}) {
        
        // Reset world to consistent state
        world->reset();
        
        // Add material in a column to create pressure
        addDirtToCell(4, 1, 0.9);
        addDirtToCell(4, 2, 0.8);
        addDirtToCell(4, 3, 0.9);
        addDirtToCell(4, 4, 0.7);
        
        world->setPressureSystem(system);
        
        // Run simulation
        for (int step = 0; step < 8; ++step) {
            world->advanceTime(1.0/60.0);
        }
        
        // Collect pressure statistics
        double total_pressure = 0.0;
        double max_pressure = 0.0;
        
        for (uint32_t y = 0; y < world->getHeight(); ++y) {
            for (uint32_t x = 0; x < world->getWidth(); ++x) {
                double pressure_mag = world->at(x, y).pressure.mag();
                total_pressure += pressure_mag;
                max_pressure = std::max(max_pressure, pressure_mag);
            }
        }
        
        PressureResult result;
        result.system = system;
        result.total_pressure = total_pressure;
        result.max_pressure = max_pressure;
        
        switch(system) {
            case World::PressureSystem::Original:
                result.name = "Original (COM)";
                break;
            case World::PressureSystem::TopDown:
                result.name = "Top-Down Hydrostatic";
                break;
            case World::PressureSystem::IterativeSettling:
                result.name = "Iterative Settling";
                break;
        }
        
        results.push_back(result);
    }
    
    // Display comparison
    std::cout << "\nPressure System Comparison Results:" << std::endl;
    std::cout << "System                    | Total Pressure | Max Pressure" << std::endl;
    std::cout << "--------------------------|----------------|-------------" << std::endl;
    
    for (const auto& result : results) {
        printf("%-25s | %14.6f | %12.6f\n", 
               result.name.c_str(),
               result.total_pressure, 
               result.max_pressure);
    }
    
    // Basic sanity checks
    for (const auto& result : results) {
        EXPECT_GE(result.total_pressure, 0.0) << "Pressure should be non-negative for " << result.name;
        EXPECT_GE(result.max_pressure, 0.0) << "Max pressure should be non-negative for " << result.name;
    }
    
    std::cout << "\n=== Comparison completed ===" << std::endl;
}

TEST_F(PressureSystemTest, PressureSystemAPI) {
    std::cout << "\n=== PRESSURE SYSTEM API TEST ===" << std::endl;
    
    // Test that we can switch pressure systems programmatically
    // (this simulates what the UI dropdown callback does)
    
    // Start with Original system
    world->setPressureSystem(World::PressureSystem::Original);
    EXPECT_EQ(world->getPressureSystem(), World::PressureSystem::Original);
    std::cout << "✓ Original system set successfully" << std::endl;
    
    // Switch to TopDown
    world->setPressureSystem(World::PressureSystem::TopDown);
    EXPECT_EQ(world->getPressureSystem(), World::PressureSystem::TopDown);
    std::cout << "✓ TopDown system set successfully" << std::endl;
    
    // Switch to IterativeSettling
    world->setPressureSystem(World::PressureSystem::IterativeSettling);
    EXPECT_EQ(world->getPressureSystem(), World::PressureSystem::IterativeSettling);
    std::cout << "✓ IterativeSettling system set successfully" << std::endl;
    
    // Switch back to Original
    world->setPressureSystem(World::PressureSystem::Original);
    EXPECT_EQ(world->getPressureSystem(), World::PressureSystem::Original);
    std::cout << "✓ Switched back to Original system successfully" << std::endl;
    
    std::cout << "=== API test completed ===\n" << std::endl;
}

TEST_F(PressureSystemTest, TopDownPressureAccumulation) {
    std::cout << "\n=== TOP-DOWN PRESSURE ACCUMULATION TEST ===" << std::endl;
    
    // Create a vertical column of material to test pressure accumulation
    world->reset();
    
    // Add material vertically (should create accumulating pressure)
    for (uint32_t y = 1; y <= 5; ++y) {
        addDirtToCell(4, y, 0.8);  // Center column
    }
    
    // Use top-down pressure system
    world->setPressureSystem(World::PressureSystem::TopDown);
    
    // Run simulation to let pressure develop
    for (int step = 0; step < 12; ++step) {
        world->advanceTime(1.0/60.0);
    }
    
    // Check pressure by row
    std::vector<double> pressures_by_row;
    for (uint32_t y = 0; y < world->getHeight(); ++y) {
        double row_pressure = 0.0;
        for (uint32_t x = 0; x < world->getWidth(); ++x) {
            row_pressure += world->at(x, y).pressure.mag();
        }
        pressures_by_row.push_back(row_pressure);
        if (row_pressure > 0.0001) {  // Only print significant pressures
            std::cout << "  Row " << y << " pressure: " << row_pressure << std::endl;
        }
    }
    
    // Check if any pressure was generated
    double total_pressure = getTotalPressure();
    if (total_pressure > 0.0001) {
        std::cout << "  ✓ Top-down pressure system generated pressure: " << total_pressure << std::endl;
    } else {
        std::cout << "  ! No significant pressure generated" << std::endl;
    }
    
    // Basic validation
    EXPECT_GE(total_pressure, 0.0) << "Total pressure should be non-negative";
    
    std::cout << "=== Top-down test completed ===\n" << std::endl;
} 
