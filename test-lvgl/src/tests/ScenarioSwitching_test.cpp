#include <gtest/gtest.h>
#include "../scenarios/ScenarioRegistry.h"
#include "../scenarios/ScenarioWorldSetup.h"
#include "../WorldFactory.h"
#include "../WorldInterface.h"
#include "../SimulationManager.h"
#include "spdlog/spdlog.h"
#include <thread>
#include <chrono>

/**
 * Unit tests specifically for scenario switching segfault debugging.
 * These tests simulate the conditions that might cause crashes when switching scenarios.
 */
class ScenarioSwitchingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear registry to ensure clean state
        ScenarioRegistry::getInstance().clear();
        
        // Register test scenarios
        registerTestScenarios();
    }
    
    void TearDown() override {
        ScenarioRegistry::getInstance().clear();
    }
    
private:
    void registerTestScenarios() {
        // Register a simple test scenario
        class TestScenario : public Scenario {
        public:
            TestScenario(const std::string& name) {
                metadata_.name = name;
                metadata_.description = "Test scenario";
                metadata_.category = "test";
                metadata_.supportsWorldB = true;
            }
            
            const ScenarioMetadata& getMetadata() const override {
                return metadata_;
            }
            
            std::unique_ptr<WorldSetup> createWorldSetup() const override {
                auto setup = std::make_unique<ScenarioWorldSetup>();
                setup->setSetupFunction([](WorldInterface& world) {
                    spdlog::debug("Test scenario setup for {}", world.getWorldType() == WorldType::RulesB ? "WorldB" : "WorldA");
                    // Add some material to test state changes
                    world.addDirtAtPixel(50, 50);
                });
                return setup;
            }
            
        private:
            ScenarioMetadata metadata_;
        };
        
        auto& registry = ScenarioRegistry::getInstance();
        registry.registerScenario("test1", std::make_unique<TestScenario>("Test Scenario 1"));
        registry.registerScenario("test2", std::make_unique<TestScenario>("Test Scenario 2"));
    }
};

// Test basic scenario switching with WorldSetup
TEST_F(ScenarioSwitchingTest, BasicWorldSetupSwitch) {
    auto world = createWorld(WorldType::RulesB, 10, 10, nullptr);
    auto& registry = ScenarioRegistry::getInstance();
    
    // Get initial mass
    double initialMass = world->getTotalMass();
    
    // Apply a scenario
    auto* scenario = registry.getScenario("test1");
    ASSERT_NE(scenario, nullptr);
    
    auto setup = scenario->createWorldSetup();
    world->setWorldSetup(std::move(setup));
    
    // Mass should have changed (scenario adds dirt)
    double afterMass = world->getTotalMass();
    EXPECT_GT(afterMass, initialMass) << "Scenario should have added material";
    
    // Switch to another scenario
    auto* scenario2 = registry.getScenario("test2");
    world->setWorldSetup(scenario2->createWorldSetup());
    
    // World should still be valid
    EXPECT_GT(world->getTotalMass(), 0.0);
}

// Test that null setup handling doesn't crash
TEST_F(ScenarioSwitchingTest, NullSetupHandling) {
    auto world = createWorld(WorldType::RulesB, 10, 10, nullptr);
    
    // Set null setup shouldn't crash
    world->setWorldSetup(nullptr);
    
    // World should still be functional
    world->advanceTime(0.016);
    EXPECT_GE(world->getTotalMass(), 0.0);
}

// Test rapid scenario switching (stress test)
TEST_F(ScenarioSwitchingTest, RapidScenarioSwitching) {
    auto world = createWorld(WorldType::RulesB, 10, 10, nullptr);
    auto& registry = ScenarioRegistry::getInstance();
    
    // Rapidly switch scenarios
    for (int i = 0; i < 10; ++i) {
        auto* scenario = registry.getScenario(i % 2 == 0 ? "test1" : "test2");
        if (scenario) {
            world->setWorldSetup(scenario->createWorldSetup());
        }
        world->advanceTime(0.016);
    }
    
    // If we get here without crashing, the test passes
    EXPECT_GE(world->getTotalMass(), 0.0);
}

// Test scenario switching during continuous physics updates
TEST_F(ScenarioSwitchingTest, ScenarioSwitchDuringPhysics) {
    auto world = createWorld(WorldType::RulesB, 10, 10, nullptr);
    auto& registry = ScenarioRegistry::getInstance();
    
    // Add initial material
    world->addDirtAtPixel(50, 50);
    world->addWaterAtPixel(60, 60);
    
    // Run physics for a few steps
    for (int i = 0; i < 5; ++i) {
        world->advanceTime(0.016);
    }
    
    // Switch scenario mid-simulation
    auto* scenario = registry.getScenario("test1");
    world->setWorldSetup(scenario->createWorldSetup());
    
    // Continue physics
    for (int i = 0; i < 5; ++i) {
        world->advanceTime(0.016);
    }
    
    // Should not have crashed
    EXPECT_GE(world->getTotalMass(), 0.0);
}

// Test scenario switching with ConfigurableWorldSetup
TEST_F(ScenarioSwitchingTest, ConfigurableWorldSetupScenario) {
    auto world = createWorld(WorldType::RulesB, 10, 10, nullptr);
    
    // Create a ConfigurableWorldSetup
    auto configurableSetup = std::make_unique<ConfigurableWorldSetup>();
    configurableSetup->setLeftThrowEnabled(true);
    configurableSetup->setRightThrowEnabled(true);
    configurableSetup->setRainRate(0.1);
    
    // Apply it
    world->setWorldSetup(std::move(configurableSetup));
    
    // Run physics to trigger rain
    for (int i = 0; i < 10; ++i) {
        world->advanceTime(0.016);
    }
    
    // Should have added some water from rain
    EXPECT_GT(world->getTotalMass(), 0.0);
}

// Test concurrent scenario switching and physics (thread safety)
TEST_F(ScenarioSwitchingTest, ConcurrentScenarioSwitchAndPhysics) {
    auto world = createWorld(WorldType::RulesB, 10, 10, nullptr);
    auto& registry = ScenarioRegistry::getInstance();
    
    std::atomic<bool> stop(false);
    std::atomic<int> switches(0);
    
    // Physics thread
    std::thread simThread([&]() {
        while (!stop) {
            world->advanceTime(0.016); // Advance ~60 FPS
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    
    // Scenario switching thread (simulates UI)
    std::thread switchThread([&]() {
        for (int i = 0; i < 10 && !stop; ++i) {
            auto* scenario = registry.getScenario(i % 2 == 0 ? "test1" : "test2");
            if (scenario) {
                world->setWorldSetup(scenario->createWorldSetup());
                switches++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // Let it run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop = true;
    
    simThread.join();
    switchThread.join();
    
    // If we get here without crashing, the test passes
    EXPECT_GT(switches.load(), 0) << "Should have completed some scenario switches";
    EXPECT_GE(world->getTotalMass(), 0.0) << "World should still be valid";
}

// Test memory ownership during scenario switch
TEST_F(ScenarioSwitchingTest, MemoryOwnershipDuringSwitch) {
    auto world = createWorld(WorldType::RulesB, 10, 10, nullptr);
    auto& registry = ScenarioRegistry::getInstance();
    
    // Get current setup pointer before switch
    auto* oldSetup = world->getWorldSetup();
    
    // Apply new scenario
    auto* scenario = registry.getScenario("test1");
    world->setWorldSetup(scenario->createWorldSetup());
    
    // Old setup should be gone, new one should be valid
    auto* newSetup = world->getWorldSetup();
    EXPECT_NE(newSetup, nullptr);
    EXPECT_NE(newSetup, oldSetup) << "Should have new WorldSetup instance";
    
    // Try to use the world after switch
    world->advanceTime(0.016);
    EXPECT_GE(world->getTotalMass(), 0.0);
}

// Test that world dimensions are restored when switching back to a scenario without specific size requirements
TEST_F(ScenarioSwitchingTest, DimensionRestorationOnScenarioSwitch) {
    // Create a SimulationManager with default dimensions
    const uint32_t defaultWidth = 8;
    const uint32_t defaultHeight = 8;
    auto manager = std::make_unique<SimulationManager>(WorldType::RulesB, defaultWidth, defaultHeight, nullptr, nullptr);
    manager->initialize();
    
    // Verify initial dimensions
    EXPECT_EQ(manager->getWidth(), defaultWidth);
    EXPECT_EQ(manager->getHeight(), defaultHeight);
    
    // Register a test scenario with specific dimensions
    class SpecificSizeScenario : public Scenario {
    public:
        SpecificSizeScenario() {
            metadata_.name = "SpecificSize";
            metadata_.description = "Test scenario with specific dimensions";
            metadata_.category = "test";
            metadata_.supportsWorldB = true;
            metadata_.requiredWidth = 3;
            metadata_.requiredHeight = 6;
        }
        
        const ScenarioMetadata& getMetadata() const override {
            return metadata_;
        }
        
        std::unique_ptr<WorldSetup> createWorldSetup() const override {
            return std::make_unique<ScenarioWorldSetup>();
        }
        
    private:
        ScenarioMetadata metadata_;
    };
    
    // Register the specific size scenario
    auto& registry = ScenarioRegistry::getInstance();
    registry.registerScenario("specific_size", std::make_unique<SpecificSizeScenario>());
    
    // Apply the specific size scenario
    auto* specificScenario = registry.getScenario("specific_size");
    ASSERT_NE(specificScenario, nullptr);
    
    // This should resize the world to 3x6
    manager->resizeWorldIfNeeded(specificScenario->getMetadata().requiredWidth, 
                                 specificScenario->getMetadata().requiredHeight);
    EXPECT_EQ(manager->getWidth(), 3);
    EXPECT_EQ(manager->getHeight(), 6);
    
    // Now switch back to a scenario without specific dimensions (like Sandbox)
    // This should restore the default dimensions
    manager->resizeWorldIfNeeded(0, 0);
    EXPECT_EQ(manager->getWidth(), defaultWidth) << "Width should be restored to default when scenario has no size requirements";
    EXPECT_EQ(manager->getHeight(), defaultHeight) << "Height should be restored to default when scenario has no size requirements";
}