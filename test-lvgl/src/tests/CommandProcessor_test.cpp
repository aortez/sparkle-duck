#include <gtest/gtest.h>
#include "../network/CommandProcessor.h"
#include "../SimulationManager.h"
#include "../World.h"
#include "../MaterialType.h"
#include "lvgl/src/libs/thorvg/rapidjson/document.h"
#include <spdlog/spdlog.h>

class CommandProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create headless simulation manager.
        manager_ = std::make_unique<SimulationManager>(
            10, 10,
            nullptr,  // No screen (headless).
            nullptr   // No event router.
        );
        manager_->initialize();

        // Create command processor.
        processor_ = std::make_unique<CommandProcessor>(manager_.get());

        // Reduce logging noise.
        spdlog::set_level(spdlog::level::warn);
    }

    void TearDown() override {
        processor_.reset();
        manager_.reset();
        spdlog::set_level(spdlog::level::info);
    }

    // Helper to parse JSON response string.
    rapidjson::Document parseResponse(const std::string& jsonStr) {
        rapidjson::Document doc;
        doc.Parse(jsonStr.c_str());
        return doc;
    }

    std::unique_ptr<SimulationManager> manager_;
    std::unique_ptr<CommandProcessor> processor_;
};

TEST_F(CommandProcessorTest, StepCommand) {
    uint32_t initial_timestep = manager_->getWorld()->getTimestep();

    auto result = processor_->processCommand(R"({"command": "step", "frames": 5})");

    ASSERT_TRUE(result.isValue());
    auto response = parseResponse(result.value());
    EXPECT_TRUE(response.HasMember("timestep"));
    EXPECT_EQ(response["timestep"].GetUint(), initial_timestep + 5);
}

TEST_F(CommandProcessorTest, StepDefaultFrames) {
    uint32_t initial_timestep = manager_->getWorld()->getTimestep();

    auto result = processor_->processCommand(R"({"command": "step"})");

    ASSERT_TRUE(result.isValue());
    auto response = parseResponse(result.value());
    EXPECT_EQ(response["timestep"].GetUint(), initial_timestep + 1);
}

TEST_F(CommandProcessorTest, PlaceMaterialSuccess) {
    // Clear the specific cell we'll use (scenario may have filled it).
    World* world = dynamic_cast<World*>(manager_->getWorld());
    world->at(3, 3) = Cell(); // Empty AIR cell.

    auto result = processor_->processCommand(
        R"({"command": "place_material", "x": 3, "y": 3, "material": "WATER", "fill": 1.0})");

    ASSERT_TRUE(result.isValue());

    // Verify material was placed.
    EXPECT_EQ(world->at(3, 3).getMaterialType(), MaterialType::WATER);
    EXPECT_DOUBLE_EQ(world->at(3, 3).getFillRatio(), 1.0);
}

TEST_F(CommandProcessorTest, PlaceMaterialPartialFill) {
    // Clear the specific cell we'll use.
    World* world = dynamic_cast<World*>(manager_->getWorld());
    world->at(4, 4) = Cell(); // Empty AIR cell.

    auto result = processor_->processCommand(
        R"({"command": "place_material", "x": 4, "y": 4, "material": "DIRT", "fill": 0.5})");

    ASSERT_TRUE(result.isValue());

    EXPECT_EQ(world->at(4, 4).getMaterialType(), MaterialType::DIRT);
    EXPECT_DOUBLE_EQ(world->at(4, 4).getFillRatio(), 0.5);
}

TEST_F(CommandProcessorTest, PlaceMaterialInvalidCoordinates) {
    auto result = processor_->processCommand(
        R"({"command": "place_material", "x": 100, "y": 100, "material": "WATER"})");

    ASSERT_TRUE(result.isError());
    EXPECT_NE(result.error().message.find("Invalid coordinates"), std::string::npos);
}

TEST_F(CommandProcessorTest, PlaceMaterialInvalidMaterial) {
    auto result = processor_->processCommand(
        R"({"command": "place_material", "x": 5, "y": 5, "material": "GOLD"})");

    ASSERT_TRUE(result.isError());
    EXPECT_NE(result.error().message.find("Invalid material"), std::string::npos);
}

TEST_F(CommandProcessorTest, PlaceMaterialMissingParameters) {
    auto result = processor_->processCommand(R"({"command": "place_material", "x": 5})");

    ASSERT_TRUE(result.isError());
    EXPECT_NE(result.error().message.find("Missing"), std::string::npos);
}

TEST_F(CommandProcessorTest, GetCellSuccess) {
    // Clear and place a cell.
    World* world = dynamic_cast<World*>(manager_->getWorld());
    world->at(4, 4) = Cell();
    world->addMaterialAtCell(4, 4, MaterialType::SAND, 0.8);

    auto result = processor_->processCommand(R"({"command": "get_cell", "x": 4, "y": 4})");

    ASSERT_TRUE(result.isValue());
    auto response = parseResponse(result.value());

    EXPECT_TRUE(response.HasMember("material_type"));
    EXPECT_TRUE(response.HasMember("fill_ratio"));
    EXPECT_STREQ(response["material_type"].GetString(), "SAND");
    EXPECT_DOUBLE_EQ(response["fill_ratio"].GetDouble(), 0.8);
}

TEST_F(CommandProcessorTest, GetCellInvalidCoordinates) {
    auto result = processor_->processCommand(R"({"command": "get_cell", "x": 50, "y": 50})");

    ASSERT_TRUE(result.isError());
    EXPECT_NE(result.error().message.find("Invalid coordinates"), std::string::npos);
}

TEST_F(CommandProcessorTest, GetStateReturnsCompleteWorld) {
    // Clear cells and add specific materials for testing.
    World* world = dynamic_cast<World*>(manager_->getWorld());
    world->at(3, 3) = Cell();
    world->at(4, 4) = Cell();

    // Add some material.
    world->addMaterialAtCell(3, 3, MaterialType::WATER, 1.0);
    world->addMaterialAtCell(4, 4, MaterialType::DIRT, 0.7);

    auto result = processor_->processCommand(R"({"command": "get_state"})");

    ASSERT_TRUE(result.isValue());
    auto response = parseResponse(result.value());

    // Verify it has world structure.
    EXPECT_TRUE(response.HasMember("grid"));
    EXPECT_TRUE(response.HasMember("physics"));
    EXPECT_TRUE(response.HasMember("cells"));

    // Verify grid metadata.
    EXPECT_EQ(response["grid"]["width"].GetUint(), 10u);
    EXPECT_EQ(response["grid"]["height"].GetUint(), 10u);

    // Verify cells array exists (may have more than our 2 cells due to scenario setup).
    EXPECT_TRUE(response["cells"].IsArray());
    EXPECT_GE(response["cells"].Size(), 2u);
}

TEST_F(CommandProcessorTest, SetGravitySuccess) {
    auto result = processor_->processCommand(R"({"command": "set_gravity", "value": 15.5})");

    ASSERT_TRUE(result.isValue());
    EXPECT_DOUBLE_EQ(manager_->getWorld()->getGravity(), 15.5);
}

TEST_F(CommandProcessorTest, SetGravityMissingValue) {
    auto result = processor_->processCommand(R"({"command": "set_gravity"})");

    ASSERT_TRUE(result.isError());
    EXPECT_NE(result.error().message.find("Missing"), std::string::npos);
}

TEST_F(CommandProcessorTest, ResetCommand) {
    // Add material and advance simulation.
    manager_->getWorld()->addMaterialAtCell(5, 5, MaterialType::WATER, 1.0);
    manager_->advanceTime(0.016);
    EXPECT_GT(manager_->getWorld()->getTimestep(), 0u);

    // Reset.
    auto result = processor_->processCommand(R"({"command": "reset"})");

    ASSERT_TRUE(result.isValue());
    // After reset, world is in initial state (timestep may vary by scenario).
}

TEST_F(CommandProcessorTest, UnknownCommand) {
    auto result = processor_->processCommand(R"({"command": "do_backflip"})");

    ASSERT_TRUE(result.isError());
    EXPECT_NE(result.error().message.find("Unknown command"), std::string::npos);
}

TEST_F(CommandProcessorTest, InvalidJSON) {
    auto result = processor_->processCommand("not valid json");

    ASSERT_TRUE(result.isError());
    EXPECT_NE(result.error().message.find("parse error"), std::string::npos);
}

TEST_F(CommandProcessorTest, MissingCommandField) {
    auto result = processor_->processCommand(R"({"foo": "bar"})");

    ASSERT_TRUE(result.isError());
    EXPECT_NE(result.error().message.find("'command'"), std::string::npos);
}

TEST_F(CommandProcessorTest, MultipleCommands) {
    // Execute a sequence of commands.

    // Step 1: Place material.
    auto r1 = processor_->processCommand(
        R"({"command": "place_material", "x": 2, "y": 2, "material": "WATER", "fill": 1.0})");
    ASSERT_TRUE(r1.isValue());

    // Step 2: Set gravity.
    auto r2 = processor_->processCommand(R"({"command": "set_gravity", "value": 20.0})");
    ASSERT_TRUE(r2.isValue());

    // Step 3: Step simulation.
    auto r3 = processor_->processCommand(R"({"command": "step", "frames": 3})");
    ASSERT_TRUE(r3.isValue());

    // Step 4: Get state.
    auto r4 = processor_->processCommand(R"({"command": "get_state"})");
    ASSERT_TRUE(r4.isValue());

    // Verify final state.
    auto state = parseResponse(r4.value());
    EXPECT_EQ(state["grid"]["timestep"].GetUint(), 3u);
    EXPECT_DOUBLE_EQ(state["physics"]["gravity"].GetDouble(), 20.0);
}
