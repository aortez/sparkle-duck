#include <gtest/gtest.h>
#include "../World.h"
#include "../Cell.h"
#include "../MaterialType.h"
#include "../Vector2d.h"
#include "lvgl/src/libs/thorvg/rapidjson/document.h"
#include "lvgl/src/libs/thorvg/rapidjson/stringbuffer.h"
#include "lvgl/src/libs/thorvg/rapidjson/writer.h"
#include <spdlog/spdlog.h>

class WorldJSONTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable verbose logging during tests.
        spdlog::set_level(spdlog::level::warn);
    }

    void TearDown() override {
        // Restore default logging.
        spdlog::set_level(spdlog::level::info);
    }

    // Helper to compare two worlds for equality.
    void validateWorldsEqual(const World& original, const World& restored) {
        // Grid dimensions.
        EXPECT_EQ(original.getWidth(), restored.getWidth());
        EXPECT_EQ(original.getHeight(), restored.getHeight());

        // Simulation state.
        EXPECT_EQ(original.getTimestep(), restored.getTimestep());
        EXPECT_DOUBLE_EQ(original.getTimescale(), restored.getTimescale());
        EXPECT_DOUBLE_EQ(original.getRemovedMass(), restored.getRemovedMass());

        // Physics parameters.
        EXPECT_DOUBLE_EQ(original.getGravity(), restored.getGravity());
        EXPECT_DOUBLE_EQ(original.getElasticityFactor(), restored.getElasticityFactor());
        EXPECT_DOUBLE_EQ(original.getWaterPressureThreshold(), restored.getWaterPressureThreshold());
        EXPECT_EQ(original.getPressureSystem(), restored.getPressureSystem());

        // Pressure controls.
        EXPECT_EQ(original.isPressureDiffusionEnabled(), restored.isPressureDiffusionEnabled());
        EXPECT_DOUBLE_EQ(
            original.getHydrostaticPressureStrength(), restored.getHydrostaticPressureStrength());
        EXPECT_DOUBLE_EQ(
            original.getDynamicPressureStrength(), restored.getDynamicPressureStrength());

        // Cohesion/adhesion/viscosity.
        EXPECT_EQ(
            original.isCohesionBindForceEnabled(), restored.isCohesionBindForceEnabled());
        EXPECT_DOUBLE_EQ(
            original.getCohesionComForceStrength(), restored.getCohesionComForceStrength());
        EXPECT_EQ(original.getCOMCohesionRange(), restored.getCOMCohesionRange());
        EXPECT_DOUBLE_EQ(original.getViscosityStrength(), restored.getViscosityStrength());
        EXPECT_DOUBLE_EQ(original.getFrictionStrength(), restored.getFrictionStrength());
        EXPECT_DOUBLE_EQ(original.getAdhesionStrength(), restored.getAdhesionStrength());
        EXPECT_EQ(original.isAdhesionEnabled(), restored.isAdhesionEnabled());

        // Air resistance.
        EXPECT_EQ(original.isAirResistanceEnabled(), restored.isAirResistanceEnabled());
        EXPECT_DOUBLE_EQ(
            original.getAirResistanceStrength(), restored.getAirResistanceStrength());

        // Setup controls.
        EXPECT_EQ(original.getSelectedMaterial(), restored.getSelectedMaterial());
        EXPECT_EQ(original.isDebugDrawEnabled(), restored.isDebugDrawEnabled());

        // Cell data - compare all cells.
        for (uint32_t y = 0; y < original.getHeight(); ++y) {
            for (uint32_t x = 0; x < original.getWidth(); ++x) {
                const Cell& origCell = original.at(x, y);
                const Cell& restCell = restored.at(x, y);

                EXPECT_EQ(origCell.getMaterialType(), restCell.getMaterialType())
                    << "Mismatch at (" << x << "," << y << ")";
                EXPECT_DOUBLE_EQ(origCell.getFillRatio(), restCell.getFillRatio())
                    << "Mismatch at (" << x << "," << y << ")";
                EXPECT_DOUBLE_EQ(origCell.getCOM().x, restCell.getCOM().x)
                    << "Mismatch at (" << x << "," << y << ")";
                EXPECT_DOUBLE_EQ(origCell.getCOM().y, restCell.getCOM().y)
                    << "Mismatch at (" << x << "," << y << ")";
                EXPECT_DOUBLE_EQ(origCell.getVelocity().x, restCell.getVelocity().x)
                    << "Mismatch at (" << x << "," << y << ")";
                EXPECT_DOUBLE_EQ(origCell.getVelocity().y, restCell.getVelocity().y)
                    << "Mismatch at (" << x << "," << y << ")";
            }
        }
    }

    // Helper to convert JSON document to string for debugging.
    std::string jsonToString(const rapidjson::Document& doc) {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);
        return buffer.GetString();
    }
};

TEST_F(WorldJSONTest, EmptyWorldSerialization) {
    World world(10, 10);
    world.reset(); // Ensure empty state.

    // Serialize.
    auto json = world.toJSON();

    // Deserialize.
    World world2(10, 10);
    world2.fromJSON(json);

    validateWorldsEqual(world, world2);
}

TEST_F(WorldJSONTest, SingleCellWorld) {
    World world(5, 5);
    world.reset();
    world.addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);

    auto json = world.toJSON();
    World world2(5, 5);
    world2.fromJSON(json);

    validateWorldsEqual(world, world2);
}

TEST_F(WorldJSONTest, MultipleCellsWithDifferentMaterials) {
    World world(10, 10);
    world.reset();
    world.addMaterialAtCell(2, 3, MaterialType::DIRT, 0.8);
    world.addMaterialAtCell(5, 7, MaterialType::WATER, 1.0);
    world.addMaterialAtCell(8, 2, MaterialType::METAL, 0.5);
    world.addMaterialAtCell(1, 9, MaterialType::SAND, 0.3);

    auto json = world.toJSON();
    World world2(10, 10);
    world2.fromJSON(json);

    validateWorldsEqual(world, world2);
}

TEST_F(WorldJSONTest, WorldAfterSimulationSteps) {
    World world(20, 20);
    world.reset();
    world.setWallsEnabled(false);
    world.addMaterialAtCell(10, 5, MaterialType::WATER, 1.0);
    world.setGravity(9.8);

    // Run simulation.
    for (int i = 0; i < 10; ++i) {
        world.advanceTime(0.016);
    }

    auto json = world.toJSON();
    World world2(20, 20);
    world2.fromJSON(json);

    validateWorldsEqual(world, world2);
}

TEST_F(WorldJSONTest, PhysicsParametersPreserved) {
    World world(5, 5);
    world.reset();

    // Set non-default physics parameters.
    world.setGravity(12.5);
    world.setElasticityFactor(0.6);
    world.setHydrostaticPressureStrength(2.0);
    world.setDynamicPressureStrength(1.5);
    world.setCohesionComForceStrength(200.0);
    world.setViscosityStrength(1.5);
    world.setFrictionStrength(0.8);
    world.setAdhesionStrength(0.7);
    world.setAirResistanceStrength(0.2);

    auto json = world.toJSON();
    World world2(5, 5);
    world2.fromJSON(json);

    validateWorldsEqual(world, world2);
}

TEST_F(WorldJSONTest, BooleanFlagsPreserved) {
    World world(5, 5);
    world.reset();

    // Toggle various flags.
    world.setPressureDiffusionEnabled(true);
    world.setCohesionBindForceEnabled(true);
    world.setAdhesionEnabled(false);
    world.setAirResistanceEnabled(false);
    world.setDebugDrawEnabled(true);

    auto json = world.toJSON();
    World world2(5, 5);
    world2.fromJSON(json);

    validateWorldsEqual(world, world2);
}

TEST_F(WorldJSONTest, MaterialSelectionPreserved) {
    World world(5, 5);
    world.reset();
    world.setSelectedMaterial(MaterialType::METAL);

    auto json = world.toJSON();
    World world2(5, 5);
    world2.fromJSON(json);

    EXPECT_EQ(world2.getSelectedMaterial(), MaterialType::METAL);
}

TEST_F(WorldJSONTest, SparseEncodingEfficiency) {
    World world(100, 100); // Large world.
    world.reset();

    // Only add a few cells.
    world.addMaterialAtCell(10, 10, MaterialType::WATER, 1.0);
    world.addMaterialAtCell(50, 50, MaterialType::DIRT, 0.8);

    auto json = world.toJSON();

    // Check that cells array is small (sparse encoding).
    EXPECT_TRUE(json.HasMember("cells"));
    EXPECT_TRUE(json["cells"].IsArray());

    // Should have exactly 2 cells in the array.
    EXPECT_EQ(json["cells"].Size(), 2u);
}

TEST_F(WorldJSONTest, JSONStructureValidation) {
    World world(5, 5);
    world.reset();

    auto json = world.toJSON();

    // Validate top-level structure.
    EXPECT_TRUE(json.IsObject());
    EXPECT_TRUE(json.HasMember("grid"));
    EXPECT_TRUE(json.HasMember("simulation"));
    EXPECT_TRUE(json.HasMember("physics"));
    EXPECT_TRUE(json.HasMember("forces"));
    EXPECT_TRUE(json.HasMember("setup"));
    EXPECT_TRUE(json.HasMember("cells"));

    // Validate section types.
    EXPECT_TRUE(json["grid"].IsObject());
    EXPECT_TRUE(json["simulation"].IsObject());
    EXPECT_TRUE(json["physics"].IsObject());
    EXPECT_TRUE(json["forces"].IsObject());
    EXPECT_TRUE(json["setup"].IsObject());
    EXPECT_TRUE(json["cells"].IsArray());
}

TEST_F(WorldJSONTest, FromJSONInvalidDocument) {
    rapidjson::Document doc;
    doc.Parse("\"not an object\"");

    World world(5, 5);
    EXPECT_THROW(world.fromJSON(doc), std::runtime_error);
}

TEST_F(WorldJSONTest, FromJSONMissingGridSection) {
    rapidjson::Document doc(rapidjson::kObjectType);
    // No grid section.

    World world(5, 5);
    EXPECT_THROW(world.fromJSON(doc), std::runtime_error);
}

TEST_F(WorldJSONTest, FromJSONMissingCellsArray) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto& allocator = doc.GetAllocator();

    rapidjson::Value grid(rapidjson::kObjectType);
    grid.AddMember("width", 5, allocator);
    grid.AddMember("height", 5, allocator);
    grid.AddMember("timestep", 0, allocator);
    doc.AddMember("grid", grid, allocator);
    // No cells array.

    World world(5, 5);
    EXPECT_THROW(world.fromJSON(doc), std::runtime_error);
}

TEST_F(WorldJSONTest, ResizeOnDeserialize) {
    // Create world with one size.
    World world(10, 10);
    world.reset();
    world.addMaterialAtCell(5, 5, MaterialType::WATER, 1.0);

    auto json = world.toJSON();

    // Deserialize into world with different size.
    World world2(20, 20);
    world2.fromJSON(json);

    // Should resize to match.
    EXPECT_EQ(world2.getWidth(), 10u);
    EXPECT_EQ(world2.getHeight(), 10u);

    // Cell data should be preserved.
    EXPECT_EQ(world2.at(5, 5).getMaterialType(), MaterialType::WATER);
    EXPECT_DOUBLE_EQ(world2.at(5, 5).getFillRatio(), 1.0);
}

TEST_F(WorldJSONTest, ComplexWorldState) {
    World world(15, 15);
    world.reset();
    world.setWallsEnabled(false);

    // Add various materials.
    world.addMaterialAtCell(5, 10, MaterialType::DIRT, 1.0);
    world.addMaterialAtCell(6, 10, MaterialType::DIRT, 0.9);
    world.addMaterialAtCell(7, 10, MaterialType::DIRT, 0.8);
    world.addMaterialAtCell(5, 5, MaterialType::WATER, 1.0);
    world.addMaterialAtCell(10, 10, MaterialType::METAL, 1.0);

    // Set physics parameters.
    world.setGravity(15.0);
    world.setElasticityFactor(0.7);
    world.setCohesionComForceStrength(250.0);
    world.setViscosityStrength(1.2);
    world.setPressureDiffusionEnabled(true);

    // Run simulation.
    for (int i = 0; i < 5; ++i) {
        world.advanceTime(0.016);
    }

    // Serialize and deserialize.
    auto json = world.toJSON();
    World world2(15, 15);
    world2.fromJSON(json);

    validateWorldsEqual(world, world2);
}

TEST_F(WorldJSONTest, TimestepPreserved) {
    World world(5, 5);
    world.reset();

    // Advance simulation.
    for (int i = 0; i < 100; ++i) {
        world.advanceTime(0.016);
    }

    uint32_t original_timestep = world.getTimestep();

    auto json = world.toJSON();
    World world2(5, 5);
    world2.fromJSON(json);

    EXPECT_EQ(world2.getTimestep(), original_timestep);
    EXPECT_EQ(world2.getTimestep(), 100u);
}

TEST_F(WorldJSONTest, CellVelocitiesPreserved) {
    World world(10, 10);
    world.reset();
    world.setWallsEnabled(false);

    // Add cell with initial velocity.
    world.addMaterialAtCell(5, 2, MaterialType::WATER, 1.0);
    world.at(5, 2).setVelocity(Vector2d{0.3, -0.5});

    auto json = world.toJSON();
    World world2(10, 10);
    world2.fromJSON(json);

    const Cell& restored_cell = world2.at(5, 2);
    EXPECT_DOUBLE_EQ(restored_cell.getVelocity().x, 0.3);
    EXPECT_DOUBLE_EQ(restored_cell.getVelocity().y, -0.5);
}

TEST_F(WorldJSONTest, CellCOMPreserved) {
    World world(10, 10);
    world.reset();

    // Add cell with specific COM.
    world.addMaterialAtCell(7, 3, MaterialType::SAND, 0.7);
    world.at(7, 3).setCOM(Vector2d{0.25, -0.15});

    auto json = world.toJSON();
    World world2(10, 10);
    world2.fromJSON(json);

    const Cell& restored_cell = world2.at(7, 3);
    EXPECT_DOUBLE_EQ(restored_cell.getCOM().x, 0.25);
    EXPECT_DOUBLE_EQ(restored_cell.getCOM().y, -0.15);
}

TEST_F(WorldJSONTest, PressureStatePreserved) {
    World world(8, 8);
    world.reset();

    // Add cells with pressure.
    world.addMaterialAtCell(4, 4, MaterialType::WATER, 1.0);
    world.at(4, 4).setPressure(15.3);
    world.at(4, 4).setHydrostaticPressure(10.0);
    world.at(4, 4).setDynamicPressure(5.3);

    auto json = world.toJSON();
    World world2(8, 8);
    world2.fromJSON(json);

    const Cell& restored_cell = world2.at(4, 4);
    EXPECT_DOUBLE_EQ(restored_cell.getPressure(), 15.3);
    EXPECT_DOUBLE_EQ(restored_cell.getHydrostaticComponent(), 10.0);
    EXPECT_DOUBLE_EQ(restored_cell.getDynamicComponent(), 5.3);
}

TEST_F(WorldJSONTest, AllMaterialTypesPreserved) {
    World world(10, 10);
    world.reset();

    // Add one of each material type.
    std::vector<MaterialType> materials = {
        MaterialType::DIRT,   MaterialType::WATER, MaterialType::WOOD,
        MaterialType::SAND,   MaterialType::METAL, MaterialType::LEAF,
        MaterialType::WALL
    };

    for (size_t i = 0; i < materials.size(); ++i) {
        world.addMaterialAtCell(i, i, materials[i], 0.9);
    }

    auto json = world.toJSON();
    World world2(10, 10);
    world2.fromJSON(json);

    for (size_t i = 0; i < materials.size(); ++i) {
        EXPECT_EQ(world2.at(i, i).getMaterialType(), materials[i]);
        EXPECT_DOUBLE_EQ(world2.at(i, i).getFillRatio(), 0.9);
    }
}

TEST_F(WorldJSONTest, EmptyCellsNotSerialized) {
    World world(20, 20);
    world.reset();

    // Add only 3 cells in a large world.
    world.addMaterialAtCell(5, 5, MaterialType::WATER, 1.0);
    world.addMaterialAtCell(10, 10, MaterialType::DIRT, 0.8);
    world.addMaterialAtCell(15, 15, MaterialType::SAND, 0.5);

    auto json = world.toJSON();

    // Verify sparse encoding - should only have 3 cells.
    EXPECT_TRUE(json.HasMember("cells"));
    EXPECT_TRUE(json["cells"].IsArray());
    EXPECT_EQ(json["cells"].Size(), 3u);
}

TEST_F(WorldJSONTest, JSONPrettyPrintable) {
    World world(5, 5);
    world.reset();
    world.addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);

    auto json = world.toJSON();
    std::string json_str = jsonToString(json);

    // Should be valid JSON string.
    EXPECT_GT(json_str.length(), 10u);

    // Should contain expected keys.
    EXPECT_NE(json_str.find("\"grid\""), std::string::npos);
    EXPECT_NE(json_str.find("\"physics\""), std::string::npos);
    EXPECT_NE(json_str.find("\"cells\""), std::string::npos);
}
