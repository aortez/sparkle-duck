#include <gtest/gtest.h>
#include "../Cell.h"
#include "../MaterialType.h"
#include "../Vector2d.h"
#include "lvgl/src/libs/thorvg/rapidjson/document.h"
#include "lvgl/src/libs/thorvg/rapidjson/stringbuffer.h"
#include "lvgl/src/libs/thorvg/rapidjson/writer.h"
#include <cmath>

class CellJSONTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup if needed.
    }

    void TearDown() override {
        // Cleanup if needed.
    }

    // Helper to validate round-trip serialization.
    void validateRoundTrip(const Cell& original) {
        rapidjson::Document doc;
        auto& allocator = doc.GetAllocator();

        // Serialize to JSON.
        rapidjson::Value json = original.toJson(allocator);

        // Deserialize back.
        Cell restored = Cell::fromJson(json);

        // Validate equality.
        EXPECT_EQ(original.getMaterialType(), restored.getMaterialType());
        EXPECT_DOUBLE_EQ(original.getFillRatio(), restored.getFillRatio());
        EXPECT_DOUBLE_EQ(original.getCOM().x, restored.getCOM().x);
        EXPECT_DOUBLE_EQ(original.getCOM().y, restored.getCOM().y);
        EXPECT_DOUBLE_EQ(original.getVelocity().x, restored.getVelocity().x);
        EXPECT_DOUBLE_EQ(original.getVelocity().y, restored.getVelocity().y);
        EXPECT_DOUBLE_EQ(original.getPressure(), restored.getPressure());
    }

    // Helper to convert JSON value to string for debugging.
    std::string jsonToString(const rapidjson::Value& json) {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        json.Accept(writer);
        return buffer.GetString();
    }
};

TEST_F(CellJSONTest, EmptyAirCellSerialization) {
    Cell cell; // Default constructor creates empty AIR cell.
    validateRoundTrip(cell);
}

TEST_F(CellJSONTest, FullWaterCellSerialization) {
    Cell cell(MaterialType::WATER, 1.0);
    validateRoundTrip(cell);
}

TEST_F(CellJSONTest, PartialDirtCellSerialization) {
    Cell cell(MaterialType::DIRT, 0.5);
    validateRoundTrip(cell);
}

TEST_F(CellJSONTest, CellWithVelocitySerialization) {
    Cell cell(MaterialType::SAND, 0.75);
    cell.setVelocity(Vector2d(1.5, -2.3));
    validateRoundTrip(cell);
}

TEST_F(CellJSONTest, CellWithCOMSerialization) {
    Cell cell(MaterialType::METAL, 1.0);
    cell.setCOM(Vector2d(0.5, -0.3));
    validateRoundTrip(cell);
}

TEST_F(CellJSONTest, CellWithPressureSerialization) {
    Cell cell(MaterialType::WATER, 0.9);
    cell.setPressure(12.5);
    validateRoundTrip(cell);
}

TEST_F(CellJSONTest, ComplexCellState) {
    Cell cell(MaterialType::DIRT, 0.65);
    cell.setCOM(Vector2d(-0.2, 0.8));
    cell.setVelocity(Vector2d(0.5, -1.2));
    cell.setPressure(8.3);
    validateRoundTrip(cell);
}

TEST_F(CellJSONTest, AllMaterialTypes) {
    // Test each material type.
    std::vector<MaterialType> materials = {
        MaterialType::AIR,
        MaterialType::DIRT,
        MaterialType::WATER,
        MaterialType::WOOD,
        MaterialType::SAND,
        MaterialType::METAL,
        MaterialType::LEAF,
        MaterialType::WALL
    };

    for (MaterialType mat : materials) {
        Cell cell(mat, 0.8);
        validateRoundTrip(cell);
    }
}

TEST_F(CellJSONTest, JSONStructureValidation) {
    Cell cell(MaterialType::WATER, 0.75);
    cell.setCOM(Vector2d(0.1, -0.2));
    cell.setVelocity(Vector2d(1.0, -0.5));

    rapidjson::Document doc;
    auto& allocator = doc.GetAllocator();

    rapidjson::Value json = cell.toJson(allocator);

    // Validate JSON structure.
    EXPECT_TRUE(json.IsObject());
    EXPECT_TRUE(json.HasMember("material_type"));
    EXPECT_TRUE(json.HasMember("fill_ratio"));
    EXPECT_TRUE(json.HasMember("com"));
    EXPECT_TRUE(json.HasMember("velocity"));
    EXPECT_TRUE(json.HasMember("pressure"));

    // Validate types.
    EXPECT_TRUE(json["material_type"].IsString());
    EXPECT_TRUE(json["fill_ratio"].IsNumber());
    EXPECT_TRUE(json["com"].IsObject());
    EXPECT_TRUE(json["velocity"].IsObject());
    EXPECT_TRUE(json["pressure"].IsNumber());

    // Validate values.
    EXPECT_STREQ(json["material_type"].GetString(), "WATER");
    EXPECT_DOUBLE_EQ(json["fill_ratio"].GetDouble(), 0.75);
}

TEST_F(CellJSONTest, MinimalCellSerialization) {
    // Test cell with minimal fill.
    Cell cell(MaterialType::SAND, Cell::MIN_FILL_THRESHOLD + 0.001);
    validateRoundTrip(cell);
}

TEST_F(CellJSONTest, MaximalCellSerialization) {
    // Test cell with maximal fill.
    Cell cell(MaterialType::WOOD, Cell::MAX_FILL_THRESHOLD);
    validateRoundTrip(cell);
}

TEST_F(CellJSONTest, ExtremeCOMValues) {
    Cell cell(MaterialType::METAL, 0.8);
    cell.setCOM(Vector2d(Cell::COM_MAX, Cell::COM_MIN));
    validateRoundTrip(cell);
}

TEST_F(CellJSONTest, HighVelocitySerialization) {
    Cell cell(MaterialType::WATER, 1.0);
    cell.setVelocity(Vector2d(0.9, -0.9)); // Near velocity limit.
    validateRoundTrip(cell);
}

TEST_F(CellJSONTest, FromJsonInvalidObject) {
    rapidjson::Document doc;
    doc.Parse("\"not an object\"");

    EXPECT_THROW(Cell::fromJson(doc), std::runtime_error);
}

TEST_F(CellJSONTest, FromJsonMissingMaterialType) {
    rapidjson::Document doc;
    doc.Parse("{\"fill_ratio\": 0.5}");

    EXPECT_THROW(Cell::fromJson(doc), std::runtime_error);
}

TEST_F(CellJSONTest, FromJsonMissingFillRatio) {
    rapidjson::Document doc;
    doc.Parse("{\"material_type\": \"WATER\"}");

    EXPECT_THROW(Cell::fromJson(doc), std::runtime_error);
}

TEST_F(CellJSONTest, PressureComponentsSerializedCorrectly) {
    Cell cell(MaterialType::WATER, 0.9);
    cell.setHydrostaticPressure(5.0);
    cell.setDynamicPressure(3.0);

    rapidjson::Document doc;
    auto& allocator = doc.GetAllocator();
    rapidjson::Value json = cell.toJson(allocator);

    // Verify components are serialized.
    EXPECT_TRUE(json.HasMember("hydrostatic_component"));
    EXPECT_TRUE(json.HasMember("dynamic_component"));
    EXPECT_DOUBLE_EQ(json["hydrostatic_component"].GetDouble(), 5.0);
    EXPECT_DOUBLE_EQ(json["dynamic_component"].GetDouble(), 3.0);

    // Verify round-trip preserves components.
    Cell restored = Cell::fromJson(json);
    EXPECT_DOUBLE_EQ(restored.getHydrostaticComponent(), 5.0);
    EXPECT_DOUBLE_EQ(restored.getDynamicComponent(), 3.0);
}
