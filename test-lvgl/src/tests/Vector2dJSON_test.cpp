#include <gtest/gtest.h>
#include "src/Vector2d.h"
#include "lvgl/src/libs/thorvg/rapidjson/document.h"
#include "lvgl/src/libs/thorvg/rapidjson/stringbuffer.h"
#include "lvgl/src/libs/thorvg/rapidjson/writer.h"
#include <cmath>
#include <limits>

class Vector2dJSONTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup if needed.
    }
    
    void TearDown() override {
        // Cleanup if needed.
    }
    
    // Helper to validate round-trip serialization.
    void validateRoundTrip(const Vector2d& original) {
        rapidjson::Document doc;
        auto& allocator = doc.GetAllocator();
        
        // Serialize to JSON.
        rapidjson::Value json = original.toJson(allocator);
        
        // Deserialize back.
        Vector2d restored = Vector2d::fromJson(json);
        
        // Validate equality.
        EXPECT_DOUBLE_EQ(original.x, restored.x);
        EXPECT_DOUBLE_EQ(original.y, restored.y);
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

TEST_F(Vector2dJSONTest, ZeroVectorSerialization) {
    Vector2d zero(0.0, 0.0);
    validateRoundTrip(zero);
}

TEST_F(Vector2dJSONTest, PositiveVectorSerialization) {
    Vector2d positive(3.14, 2.71);
    validateRoundTrip(positive);
}

TEST_F(Vector2dJSONTest, NegativeVectorSerialization) {
    Vector2d negative(-1.5, -2.8);
    validateRoundTrip(negative);
}

TEST_F(Vector2dJSONTest, MixedSignVectorSerialization) {
    Vector2d mixed(1.23, -4.56);
    validateRoundTrip(mixed);
}

TEST_F(Vector2dJSONTest, LargeValueSerialization) {
    Vector2d large(1e6, -1e6);
    validateRoundTrip(large);
}

TEST_F(Vector2dJSONTest, SmallValueSerialization) {
    Vector2d small(1e-6, -1e-6);
    validateRoundTrip(small);
}

TEST_F(Vector2dJSONTest, JSONStructureValidation) {
    Vector2d vec(1.5, -2.5);
    rapidjson::Document doc;
    auto& allocator = doc.GetAllocator();
    
    rapidjson::Value json = vec.toJson(allocator);
    
    // Validate JSON structure.
    EXPECT_TRUE(json.IsObject());
    EXPECT_TRUE(json.HasMember("x"));
    EXPECT_TRUE(json.HasMember("y"));
    EXPECT_TRUE(json["x"].IsNumber());
    EXPECT_TRUE(json["y"].IsNumber());
    EXPECT_DOUBLE_EQ(json["x"].GetDouble(), 1.5);
    EXPECT_DOUBLE_EQ(json["y"].GetDouble(), -2.5);
}

TEST_F(Vector2dJSONTest, FromJsonInvalidObject) {
    rapidjson::Document doc;
    doc.Parse("\"not an object\"");
    
    EXPECT_THROW(Vector2d::fromJson(doc), std::runtime_error);
}

TEST_F(Vector2dJSONTest, FromJsonMissingXMember) {
    rapidjson::Document doc;
    doc.Parse("{\"y\": 2.0}");
    
    EXPECT_THROW(Vector2d::fromJson(doc), std::runtime_error);
}

TEST_F(Vector2dJSONTest, FromJsonMissingYMember) {
    rapidjson::Document doc;
    doc.Parse("{\"x\": 1.0}");
    
    EXPECT_THROW(Vector2d::fromJson(doc), std::runtime_error);
}

TEST_F(Vector2dJSONTest, FromJsonNonNumericX) {
    rapidjson::Document doc;
    doc.Parse("{\"x\": \"not a number\", \"y\": 2.0}");
    
    EXPECT_THROW(Vector2d::fromJson(doc), std::runtime_error);
}

TEST_F(Vector2dJSONTest, FromJsonNonNumericY) {
    rapidjson::Document doc;
    doc.Parse("{\"x\": 1.0, \"y\": \"not a number\"}");
    
    EXPECT_THROW(Vector2d::fromJson(doc), std::runtime_error);
}

TEST_F(Vector2dJSONTest, SpecialFloatValues) {
    // Test infinity (should work with JSON).
    Vector2d inf_vec(std::numeric_limits<double>::infinity(), 
                     -std::numeric_limits<double>::infinity());
    
    rapidjson::Document doc;
    auto& allocator = doc.GetAllocator();
    
    // Serialize to JSON.
    rapidjson::Value json = inf_vec.toJson(allocator);
    
    // RapidJSON represents infinity as null, so we just verify no crash.
    EXPECT_TRUE(json.IsObject());
    EXPECT_TRUE(json.HasMember("x"));
    EXPECT_TRUE(json.HasMember("y"));
}

TEST_F(Vector2dJSONTest, PrecisionPreservation) {
    // Test high precision values.
    Vector2d precise(1.23456789012345, -9.87654321098765);
    
    rapidjson::Document doc;
    auto& allocator = doc.GetAllocator();
    
    rapidjson::Value json = precise.toJson(allocator);
    Vector2d restored = Vector2d::fromJson(json);
    
    // Should preserve reasonable precision.
    EXPECT_NEAR(precise.x, restored.x, 1e-15);
    EXPECT_NEAR(precise.y, restored.y, 1e-15);
}