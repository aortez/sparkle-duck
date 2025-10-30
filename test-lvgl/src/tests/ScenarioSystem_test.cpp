#include <gtest/gtest.h>
#include "../scenarios/ScenarioRegistry.h"
#include "../scenarios/Scenario.h"
#include "../scenarios/ScenarioWorldSetup.h"
#include "../WorldInterface.h"
#include "../World.h"
#include "../WorldSetup.h"
#include "spdlog/spdlog.h"

/**
 * Test suite for the scenario system.
 */
class ScenarioSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear registry before each test
        ScenarioRegistry::getInstance().clear();
    }
    
    void TearDown() override {
        // Clear registry after each test
        ScenarioRegistry::getInstance().clear();
    }
};

TEST_F(ScenarioSystemTest, RegistryStartsEmpty) {
    auto& registry = ScenarioRegistry::getInstance();
    EXPECT_EQ(registry.getScenarioIds().size(), 0);
}

TEST_F(ScenarioSystemTest, CanRegisterAndRetrieveScenario) {
    // Create a simple test scenario
    class TestScenario : public Scenario {
    public:
        TestScenario() {
            metadata_.name = "Test";
            metadata_.description = "Test scenario";
            metadata_.category = "test";
        }
        
        const ScenarioMetadata& getMetadata() const override {
            return metadata_;
        }
        
        std::unique_ptr<WorldSetup> createWorldSetup() const override {
            return std::make_unique<DefaultWorldSetup>();
        }
        
    private:
        ScenarioMetadata metadata_;
    };
    
    auto& registry = ScenarioRegistry::getInstance();
    registry.registerScenario("test", std::make_unique<TestScenario>());
    
    // Verify we can retrieve it
    auto* scenario = registry.getScenario("test");
    ASSERT_NE(scenario, nullptr);
    EXPECT_EQ(scenario->getMetadata().name, "Test");
    
    // Verify it appears in the ID list
    auto ids = registry.getScenarioIds();
    EXPECT_EQ(ids.size(), 1);
    EXPECT_EQ(ids[0], "test");
}

TEST_F(ScenarioSystemTest, FilterByWorldType) {
    // Create scenarios with different world type support
    class WorldAOnlyScenario : public Scenario {
    public:
        WorldAOnlyScenario() {
            metadata_.name = "WorldA Only";
            metadata_.description = "Only works with WorldA";
            metadata_.category = "test";
            metadata_.supportsWorldA = true;
            metadata_.supportsWorldB = false;
        }
        
        const ScenarioMetadata& getMetadata() const override {
            return metadata_;
        }
        
        std::unique_ptr<WorldSetup> createWorldSetup() const override {
            return std::make_unique<DefaultWorldSetup>();
        }
        
    private:
        ScenarioMetadata metadata_;
    };
    
    auto& registry = ScenarioRegistry::getInstance();
    registry.registerScenario("worlda_only", std::make_unique<WorldAOnlyScenario>());
    
    // Test filtering
    auto worldBScenarios = registry.getScenariosForWorldType(true); // World
    EXPECT_EQ(worldBScenarios.size(), 0);
    
    auto worldAScenarios = registry.getScenariosForWorldType(false); // WorldA
    EXPECT_EQ(worldAScenarios.size(), 1);
    EXPECT_EQ(worldAScenarios[0], "worlda_only");
}

TEST_F(ScenarioSystemTest, CanApplyScenarioToWorld) {
    // Register the empty scenario manually for testing
    // (normally done by self-registration)
    ScenarioRegistry::getInstance().clear(); // Ensure clean state
    
    // Create a simple scenario
    class SimpleScenario : public Scenario {
    public:
        SimpleScenario() {
            metadata_.name = "Simple";
            metadata_.description = "Simple test scenario";
            metadata_.category = "test";
        }
        
        const ScenarioMetadata& getMetadata() const override {
            return metadata_;
        }
        
        std::unique_ptr<WorldSetup> createWorldSetup() const override {
            auto setup = std::make_unique<ScenarioWorldSetup>();
            setup->setSetupFunction([](WorldInterface& world) {
                // Disable walls to verify the setup ran
                world.setWallsEnabled(false);
            });
            return setup;
        }
        
    private:
        ScenarioMetadata metadata_;
    };
    
    auto& registry = ScenarioRegistry::getInstance();
    registry.registerScenario("simple", std::make_unique<SimpleScenario>());
    
    // Create a world and apply the scenario
    auto world = std::make_unique<World>(10, 10);
    
    // Get the scenario and apply it
    auto* scenario = registry.getScenario("simple");
    ASSERT_NE(scenario, nullptr);
    
    auto worldSetup = scenario->createWorldSetup();
    
    // Get initial state before applying scenario
    world->setup(); // Ensure world is in default state
    bool wallsBeforeScenario = world->areWallsEnabled();
    EXPECT_TRUE(wallsBeforeScenario); // Default should have walls enabled
    
    // Apply the scenario
    world->setWorldSetup(std::move(worldSetup));
    
    // Verify the setup was applied (walls should be disabled)
    EXPECT_FALSE(world->areWallsEnabled());
}