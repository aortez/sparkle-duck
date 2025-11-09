#include "core/Vector2i.h"
#include <cmath>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

using namespace DirtSim;

bool almostEqualI(double a, double b, double epsilon = 1e-6)
{
    return std::abs(a - b) < epsilon;
}

bool almostEqual(const Vector2i& a, const Vector2i& b)
{
    return a.x == b.x && a.y == b.y;
}

TEST(Vector2iTest, Constructors)
{
    spdlog::info("Starting Vector2iTest::Constructors test");
    Vector2i v1;
    EXPECT_EQ(v1.x, 0);
    EXPECT_EQ(v1.y, 0);

    Vector2i v2(1, 2);
    EXPECT_EQ(v2.x, 1);
    EXPECT_EQ(v2.y, 2);
}

TEST(Vector2iTest, Operators)
{
    spdlog::info("Starting Vector2iTest::Operators test");
    Vector2i v1(1, 2);
    Vector2i v2(3, 4);

    Vector2i sum = v1 + v2;
    EXPECT_TRUE(almostEqual(sum, Vector2i(4, 6)));

    Vector2i diff = v2 - v1;
    EXPECT_TRUE(almostEqual(diff, Vector2i(2, 2)));

    Vector2i scaled = v1 * 2;
    EXPECT_TRUE(almostEqual(scaled, Vector2i(2, 4)));

    Vector2i divided = v2 / 2;
    EXPECT_TRUE(almostEqual(divided, Vector2i(1, 2)));

    v1 += v2;
    EXPECT_TRUE(almostEqual(v1, Vector2i(4, 6)));

    v1 -= v2;
    EXPECT_TRUE(almostEqual(v1, Vector2i(1, 2)));

    v1 *= 2;
    EXPECT_TRUE(almostEqual(v1, Vector2i(2, 4)));

    v1 /= 2;
    EXPECT_TRUE(almostEqual(v1, Vector2i(1, 2)));

    EXPECT_TRUE(v1 == Vector2i(1, 2));
    EXPECT_FALSE(v1 == v2);
}

TEST(Vector2iTest, VectorOperations)
{
    spdlog::info("Starting Vector2iTest::VectorOperations test");
    Vector2i v1(3, 4);
    Vector2i v2(1, 2);

    EXPECT_TRUE(almostEqualI(v1.mag(), 5.0));

    EXPECT_EQ(v1.dot(v2), 11);

    Vector2i normalized = v1.normalize();
    // Note: Integer normalization is imprecise, so we just verify it's close to unit length.
    EXPECT_TRUE(normalized.mag() < 2.0 && normalized.mag() > 0.5);
    EXPECT_TRUE(almostEqual(normalized, Vector2i(1, 1)));

    Vector2i sum = v1.add(v2);
    EXPECT_TRUE(almostEqual(sum, Vector2i(4, 6)));

    Vector2i diff = v1.subtract(v2);
    EXPECT_TRUE(almostEqual(diff, Vector2i(2, 2)));

    Vector2i scaled = v1.times(2);
    EXPECT_TRUE(almostEqual(scaled, Vector2i(6, 8)));
}

TEST(Vector2iTest, EdgeCases)
{
    spdlog::info("Starting Vector2iTest::EdgeCases test");
    Vector2i v(1, 2);

    EXPECT_THROW(v / 0, std::runtime_error);

    Vector2i zero;
    Vector2i normalized = zero.normalize();
    EXPECT_TRUE(almostEqual(normalized, zero));
}