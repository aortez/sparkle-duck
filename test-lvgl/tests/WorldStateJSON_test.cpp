#include <gtest/gtest.h>
#include "src/WorldState.h"
#include "lvgl/src/libs/thorvg/rapidjson/document.h"
#include "lvgl/src/libs/thorvg/rapidjson/stringbuffer.h"
#include "lvgl/src/libs/thorvg/rapidjson/writer.h"

class WorldStateJSONTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup if needed
    }
    
    void TearDown() override {
        // Cleanup if needed
    }
    
    // Helper to validate round-trip serialization
    void validateRoundTrip(const WorldState& original) {
        rapidjson::Document doc;
        auto& allocator = doc.GetAllocator();
        
        // Serialize to JSON
        rapidjson::Value json = original.toJson(allocator);
        
        // Deserialize back
        WorldState restored = WorldState::fromJson(json);
        
        // Validate core properties
        EXPECT_EQ(original.width, restored.width);
        EXPECT_EQ(original.height, restored.height);
        EXPECT_EQ(original.timestep, restored.timestep);
        EXPECT_DOUBLE_EQ(original.gravity, restored.gravity);
        EXPECT_DOUBLE_EQ(original.timescale, restored.timescale);
        EXPECT_DOUBLE_EQ(original.elasticity_factor, restored.elasticity_factor);
        EXPECT_DOUBLE_EQ(original.pressure_scale, restored.pressure_scale);
        
        // Validate flags
        EXPECT_EQ(original.left_throw_enabled, restored.left_throw_enabled);
        EXPECT_EQ(original.walls_enabled, restored.walls_enabled);
        EXPECT_EQ(original.time_reversal_enabled, restored.time_reversal_enabled);
        
        // Validate grid dimensions match
        EXPECT_EQ(original.grid_data.size(), restored.grid_data.size());
        if (!original.grid_data.empty()) {
            EXPECT_EQ(original.grid_data[0].size(), restored.grid_data[0].size());
        }
    }
    
    // Helper to validate cell data round trip
    void validateCellDataRoundTrip(const WorldState::CellData& original) {
        rapidjson::Document doc;
        auto& allocator = doc.GetAllocator();
        
        // Serialize to JSON
        rapidjson::Value json = original.toJson(allocator);
        
        // Deserialize back
        WorldState::CellData restored = WorldState::CellData::fromJson(json);
        
        // Validate equality
        EXPECT_DOUBLE_EQ(original.material_mass, restored.material_mass);
        EXPECT_EQ(original.dominant_material, restored.dominant_material);
        EXPECT_DOUBLE_EQ(original.velocity.x, restored.velocity.x);
        EXPECT_DOUBLE_EQ(original.velocity.y, restored.velocity.y);
        EXPECT_DOUBLE_EQ(original.com.x, restored.com.x);
        EXPECT_DOUBLE_EQ(original.com.y, restored.com.y);
    }
    
    // Helper to convert JSON value to string for debugging
    std::string jsonToString(const rapidjson::Value& json) {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        json.Accept(writer);
        return buffer.GetString();
    }
};

TEST_F(WorldStateJSONTest, CellDataSerialization) {
    // Test empty cell data
    WorldState::CellData empty;
    validateCellDataRoundTrip(empty);
    
    // Test cell with dirt
    WorldState::CellData dirt(0.75, MaterialType::DIRT, Vector2d(0.1, -0.2), Vector2d(0.05, 0.03));
    validateCellDataRoundTrip(dirt);
    
    // Test cell with water
    WorldState::CellData water(0.9, MaterialType::WATER, Vector2d(-0.3, 0.4), Vector2d(-0.1, 0.1));
    validateCellDataRoundTrip(water);
    
    // Test cell with all material types
    MaterialType types[] = {
        MaterialType::AIR, MaterialType::DIRT, MaterialType::WATER, MaterialType::WOOD,
        MaterialType::SAND, MaterialType::METAL, MaterialType::LEAF, MaterialType::WALL
    };
    
    for (MaterialType type : types) {
        WorldState::CellData cell(0.5, type, Vector2d(0.2, -0.1), Vector2d(0.0, 0.0));
        validateCellDataRoundTrip(cell);
    }
}

TEST_F(WorldStateJSONTest, EmptyWorldStateSerialization) {
    WorldState empty;
    validateRoundTrip(empty);
}

TEST_F(WorldStateJSONTest, SmallWorldStateSerialization) {
    WorldState small(10, 8);
    small.gravity = 9.81;
    small.timescale = 1.5;
    small.timestep = 42;
    small.walls_enabled = true;
    small.time_reversal_enabled = false;
    
    validateRoundTrip(small);
}

TEST_F(WorldStateJSONTest, WorldStateWithCellData) {
    WorldState world(5, 5);
    world.gravity = 10.0;
    world.elasticity_factor = 0.7;
    world.timestep = 100;
    
    // Add some material to specific cells
    world.setCellData(1, 1, WorldState::CellData(0.8, MaterialType::DIRT, Vector2d(0.1, 0.0)));
    world.setCellData(2, 2, WorldState::CellData(0.6, MaterialType::WATER, Vector2d(-0.2, 0.3)));
    world.setCellData(3, 1, WorldState::CellData(0.9, MaterialType::SAND, Vector2d(0.0, -0.1)));
    
    validateRoundTrip(world);
    
    // Validate specific cell content after round-trip
    rapidjson::Document doc;
    auto& allocator = doc.GetAllocator();
    rapidjson::Value json = world.toJson(allocator);
    WorldState restored = WorldState::fromJson(json);
    
    // Check that non-empty cells are preserved
    const auto& cell11 = restored.getCellData(1, 1);
    EXPECT_DOUBLE_EQ(cell11.material_mass, 0.8);
    EXPECT_EQ(cell11.dominant_material, MaterialType::DIRT);
    
    const auto& cell22 = restored.getCellData(2, 2);
    EXPECT_DOUBLE_EQ(cell22.material_mass, 0.6);
    EXPECT_EQ(cell22.dominant_material, MaterialType::WATER);
    
    // Check that empty cells remain empty (default values)
    const auto& cell00 = restored.getCellData(0, 0);
    EXPECT_DOUBLE_EQ(cell00.material_mass, 0.0);
    EXPECT_EQ(cell00.dominant_material, MaterialType::AIR);
}

TEST_F(WorldStateJSONTest, JSONStructureValidation) {
    WorldState world(3, 3);
    world.gravity = 5.0;
    world.timestep = 25;
    world.setCellData(1, 1, WorldState::CellData(0.5, MaterialType::WOOD));
    
    rapidjson::Document doc;
    auto& allocator = doc.GetAllocator();
    rapidjson::Value json = world.toJson(allocator);
    
    // Validate top-level structure
    EXPECT_TRUE(json.IsObject());
    EXPECT_TRUE(json.HasMember("metadata"));
    EXPECT_TRUE(json.HasMember("grid"));
    EXPECT_TRUE(json.HasMember("physics"));
    EXPECT_TRUE(json.HasMember("setup"));
    EXPECT_TRUE(json.HasMember("cells"));
    
    // Validate grid section
    const auto& grid = json["grid"];
    EXPECT_TRUE(grid.IsObject());
    EXPECT_TRUE(grid.HasMember("width"));
    EXPECT_TRUE(grid.HasMember("height"));
    EXPECT_TRUE(grid.HasMember("timestep"));
    EXPECT_EQ(grid["width"].GetUint(), 3);
    EXPECT_EQ(grid["height"].GetUint(), 3);
    EXPECT_EQ(grid["timestep"].GetUint(), 25);
    
    // Validate physics section
    const auto& physics = json["physics"];
    EXPECT_TRUE(physics.IsObject());
    EXPECT_TRUE(physics.HasMember("gravity"));
    EXPECT_DOUBLE_EQ(physics["gravity"].GetDouble(), 5.0);
    
    // Validate cells array
    const auto& cells = json["cells"];
    EXPECT_TRUE(cells.IsArray());
    EXPECT_EQ(cells.Size(), 1);  // Only one non-empty cell
    
    const auto& cell = cells[0];
    EXPECT_TRUE(cell.HasMember("x"));
    EXPECT_TRUE(cell.HasMember("y"));
    EXPECT_TRUE(cell.HasMember("data"));
    EXPECT_EQ(cell["x"].GetUint(), 1);
    EXPECT_EQ(cell["y"].GetUint(), 1);
}

TEST_F(WorldStateJSONTest, CellDataJSONStructure) {
    WorldState::CellData cell(0.7, MaterialType::METAL, Vector2d(0.3, -0.4), Vector2d(0.1, 0.2));
    
    rapidjson::Document doc;
    auto& allocator = doc.GetAllocator();
    rapidjson::Value json = cell.toJson(allocator);
    
    // Validate CellData JSON structure
    EXPECT_TRUE(json.IsObject());
    EXPECT_TRUE(json.HasMember("material_mass"));
    EXPECT_TRUE(json.HasMember("dominant_material"));
    EXPECT_TRUE(json.HasMember("velocity"));
    EXPECT_TRUE(json.HasMember("com"));
    EXPECT_FALSE(json.HasMember("pressure"));  // Should not have pressure field
    
    EXPECT_DOUBLE_EQ(json["material_mass"].GetDouble(), 0.7);
    EXPECT_STREQ(json["dominant_material"].GetString(), "METAL");
    
    // Validate nested Vector2d objects
    const auto& velocity = json["velocity"];
    EXPECT_TRUE(velocity.IsObject());
    EXPECT_TRUE(velocity.HasMember("x"));
    EXPECT_TRUE(velocity.HasMember("y"));
    EXPECT_DOUBLE_EQ(velocity["x"].GetDouble(), 0.3);
    EXPECT_DOUBLE_EQ(velocity["y"].GetDouble(), -0.4);
    
    const auto& com = json["com"];
    EXPECT_TRUE(com.IsObject());
    EXPECT_DOUBLE_EQ(com["x"].GetDouble(), 0.1);
    EXPECT_DOUBLE_EQ(com["y"].GetDouble(), 0.2);
}

TEST_F(WorldStateJSONTest, InvalidJSONHandling) {
    rapidjson::Document doc;
    
    // Test invalid top-level JSON
    doc.Parse("\"not an object\"");
    EXPECT_THROW(WorldState::fromJson(doc), std::runtime_error);
    
    doc.Parse("[]");
    EXPECT_THROW(WorldState::fromJson(doc), std::runtime_error);
    
    doc.Parse("null");
    EXPECT_THROW(WorldState::fromJson(doc), std::runtime_error);
    
    // Test missing required fields
    doc.Parse("{}");
    EXPECT_THROW(WorldState::fromJson(doc), std::runtime_error);
    
    doc.Parse("{\"grid\": {}, \"physics\": {}}");
    EXPECT_THROW(WorldState::fromJson(doc), std::runtime_error);
}

TEST_F(WorldStateJSONTest, InvalidCellDataJSON) {
    rapidjson::Document doc;
    
    // Test invalid CellData JSON
    doc.Parse("\"not an object\"");
    EXPECT_THROW(WorldState::CellData::fromJson(doc), std::runtime_error);
    
    doc.Parse("{}");  // Missing required fields
    EXPECT_THROW(WorldState::CellData::fromJson(doc), std::runtime_error);
    
    // Test with some fields but not all
    doc.Parse("{\"material_mass\": 0.5}");
    EXPECT_THROW(WorldState::CellData::fromJson(doc), std::runtime_error);
}

TEST_F(WorldStateJSONTest, LargeGridEfficiency) {
    // Test with a larger grid to ensure reasonable performance
    WorldState large(50, 40);
    large.gravity = 8.5;
    large.timestep = 500;
    
    // Add some scattered material
    for (int i = 0; i < 100; i += 5) {
        int x = i % large.width;
        int y = (i / large.width) % large.height;
        MaterialType mat = static_cast<MaterialType>(i % 8);
        large.setCellData(x, y, WorldState::CellData(0.3 + (i % 5) * 0.1, mat));
    }
    
    // Should complete without issues
    validateRoundTrip(large);
    
    // Verify only non-empty cells are serialized
    rapidjson::Document doc;
    auto& allocator = doc.GetAllocator();
    rapidjson::Value json = large.toJson(allocator);
    
    const auto& cells = json["cells"];
    EXPECT_TRUE(cells.IsArray());
    EXPECT_EQ(cells.Size(), 20);  // 100 cells / 5 = 20 non-empty cells
}