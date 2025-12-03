#include "core/Cell.h"
#include <gtest/gtest.h>
#include <zpp_bits.h>

using namespace DirtSim;

TEST(CellSerializationTest, SupportFlagsAreSerializedWithZppBits)
{
    // Create a cell with support flags set.
    Cell original;
    original.material_type = MaterialType::DIRT;
    original.fill_ratio = 0.8;
    original.has_any_support = true;
    original.has_vertical_support = true;

    // Serialize using zpp_bits (same as network protocol).
    std::vector<std::byte> buffer;
    auto out = zpp::bits::out(buffer);
    out(original).or_throw();

    // Deserialize.
    Cell deserialized;
    auto in = zpp::bits::in(buffer);
    in(deserialized).or_throw();

    // Verify support flags survived serialization.
    EXPECT_EQ(deserialized.has_any_support, original.has_any_support)
        << "has_any_support flag was lost during zpp_bits serialization";
    EXPECT_EQ(deserialized.has_vertical_support, original.has_vertical_support)
        << "has_vertical_support flag was lost during zpp_bits serialization";

    // Also verify other fields to ensure basic serialization works.
    EXPECT_EQ(deserialized.material_type, original.material_type);
    EXPECT_DOUBLE_EQ(deserialized.fill_ratio, original.fill_ratio);
}
