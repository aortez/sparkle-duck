#include <gtest/gtest.h>
#include "src/MaterialType.h"
#include "lvgl/src/libs/thorvg/rapidjson/document.h"
#include "lvgl/src/libs/thorvg/rapidjson/stringbuffer.h"
#include "lvgl/src/libs/thorvg/rapidjson/writer.h"

class MaterialTypeJSONTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup if needed.
    }
    
    void TearDown() override {
        // Cleanup if needed.
    }
    
    // Helper to validate round-trip serialization.
    void validateRoundTrip(MaterialType original) {
        rapidjson::Document doc;
        auto& allocator = doc.GetAllocator();
        
        // Serialize to JSON.
        rapidjson::Value json = materialTypeToJson(original, allocator);
        
        // Deserialize back.
        MaterialType restored = materialTypeFromJson(json);
        
        // Validate equality.
        EXPECT_EQ(original, restored);
    }
    
    // Helper to convert JSON value to string for debugging.
    std::string jsonToString(const rapidjson::Value& json) {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        json.Accept(writer);
        return buffer.GetString();
    }
};

TEST_F(MaterialTypeJSONTest, AllMaterialTypesSerialization) {
    // Test all 8 material types.
    validateRoundTrip(MaterialType::AIR);
    validateRoundTrip(MaterialType::DIRT);
    validateRoundTrip(MaterialType::WATER);
    validateRoundTrip(MaterialType::WOOD);
    validateRoundTrip(MaterialType::SAND);
    validateRoundTrip(MaterialType::METAL);
    validateRoundTrip(MaterialType::LEAF);
    validateRoundTrip(MaterialType::WALL);
}

TEST_F(MaterialTypeJSONTest, JSONStringFormat) {
    rapidjson::Document doc;
    auto& allocator = doc.GetAllocator();
    
    // Test that materials serialize to expected string values.
    EXPECT_STREQ(materialTypeToJson(MaterialType::AIR, allocator).GetString(), "AIR");
    EXPECT_STREQ(materialTypeToJson(MaterialType::DIRT, allocator).GetString(), "DIRT");
    EXPECT_STREQ(materialTypeToJson(MaterialType::WATER, allocator).GetString(), "WATER");
    EXPECT_STREQ(materialTypeToJson(MaterialType::WOOD, allocator).GetString(), "WOOD");
    EXPECT_STREQ(materialTypeToJson(MaterialType::SAND, allocator).GetString(), "SAND");
    EXPECT_STREQ(materialTypeToJson(MaterialType::METAL, allocator).GetString(), "METAL");
    EXPECT_STREQ(materialTypeToJson(MaterialType::LEAF, allocator).GetString(), "LEAF");
    EXPECT_STREQ(materialTypeToJson(MaterialType::WALL, allocator).GetString(), "WALL");
}

TEST_F(MaterialTypeJSONTest, FromJsonValidStrings) {
    rapidjson::Document doc;
    
    // Test parsing each valid material type string.
    doc.Parse("\"AIR\"");
    EXPECT_EQ(materialTypeFromJson(doc), MaterialType::AIR);
    
    doc.Parse("\"DIRT\"");
    EXPECT_EQ(materialTypeFromJson(doc), MaterialType::DIRT);
    
    doc.Parse("\"WATER\"");
    EXPECT_EQ(materialTypeFromJson(doc), MaterialType::WATER);
    
    doc.Parse("\"WOOD\"");
    EXPECT_EQ(materialTypeFromJson(doc), MaterialType::WOOD);
    
    doc.Parse("\"SAND\"");
    EXPECT_EQ(materialTypeFromJson(doc), MaterialType::SAND);
    
    doc.Parse("\"METAL\"");
    EXPECT_EQ(materialTypeFromJson(doc), MaterialType::METAL);
    
    doc.Parse("\"LEAF\"");
    EXPECT_EQ(materialTypeFromJson(doc), MaterialType::LEAF);
    
    doc.Parse("\"WALL\"");
    EXPECT_EQ(materialTypeFromJson(doc), MaterialType::WALL);
}

TEST_F(MaterialTypeJSONTest, FromJsonInvalidType) {
    rapidjson::Document doc;
    
    // Test with non-string JSON value.
    doc.Parse("123");
    EXPECT_THROW(materialTypeFromJson(doc), std::runtime_error);
    
    doc.Parse("true");
    EXPECT_THROW(materialTypeFromJson(doc), std::runtime_error);
    
    doc.Parse("null");
    EXPECT_THROW(materialTypeFromJson(doc), std::runtime_error);
    
    doc.Parse("{}");
    EXPECT_THROW(materialTypeFromJson(doc), std::runtime_error);
    
    doc.Parse("[]");
    EXPECT_THROW(materialTypeFromJson(doc), std::runtime_error);
}

TEST_F(MaterialTypeJSONTest, FromJsonUnknownMaterial) {
    rapidjson::Document doc;
    
    // Test with unknown material type strings.
    doc.Parse("\"UNKNOWN\"");
    EXPECT_THROW(materialTypeFromJson(doc), std::runtime_error);
    
    doc.Parse("\"FIRE\"");
    EXPECT_THROW(materialTypeFromJson(doc), std::runtime_error);
    
    doc.Parse("\"PLASTIC\"");
    EXPECT_THROW(materialTypeFromJson(doc), std::runtime_error);
    
    doc.Parse("\"\"");  // Empty string.
    EXPECT_THROW(materialTypeFromJson(doc), std::runtime_error);
}

TEST_F(MaterialTypeJSONTest, CaseSensitivity) {
    rapidjson::Document doc;
    
    // Material type names should be case sensitive.
    doc.Parse("\"air\"");  // lowercase.
    EXPECT_THROW(materialTypeFromJson(doc), std::runtime_error);
    
    doc.Parse("\"Dirt\"");  // mixed case.
    EXPECT_THROW(materialTypeFromJson(doc), std::runtime_error);
    
    doc.Parse("\"WATER \"");  // trailing space.
    EXPECT_THROW(materialTypeFromJson(doc), std::runtime_error);
    
    doc.Parse("\" WATER\"");  // leading space.  
    EXPECT_THROW(materialTypeFromJson(doc), std::runtime_error);
}

TEST_F(MaterialTypeJSONTest, JSONStructureValidation) {
    rapidjson::Document doc;
    auto& allocator = doc.GetAllocator();
    
    // Test that serialized values are proper JSON strings.
    auto json = materialTypeToJson(MaterialType::DIRT, allocator);
    
    EXPECT_TRUE(json.IsString());
    EXPECT_FALSE(json.IsNull());
    EXPECT_FALSE(json.IsNumber());
    EXPECT_FALSE(json.IsObject());
    EXPECT_FALSE(json.IsArray());
    EXPECT_FALSE(json.IsBool());
    
    // Should have non-zero length.
    EXPECT_GT(strlen(json.GetString()), 0);
}

TEST_F(MaterialTypeJSONTest, ConsistencyWithMaterialNames) {
    rapidjson::Document doc;
    auto& allocator = doc.GetAllocator();
    
    // Verify JSON serialization matches getMaterialName().
    MaterialType types[] = {
        MaterialType::AIR, MaterialType::DIRT, MaterialType::WATER, MaterialType::WOOD,
        MaterialType::SAND, MaterialType::METAL, MaterialType::LEAF, MaterialType::WALL
    };
    
    for (MaterialType type : types) {
        auto json = materialTypeToJson(type, allocator);
        const char* name = getMaterialName(type);
        
        EXPECT_STREQ(json.GetString(), name) 
            << "JSON serialization should match getMaterialName() for material type " 
            << static_cast<int>(type);
    }
}