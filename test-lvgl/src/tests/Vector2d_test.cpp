#include "core/Vector2d.h"
#include <cmath>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

using namespace DirtSim;

// Helper function to compare doubles with epsilon.
bool almostEqual(double a, double b, double epsilon = 1e-6)
{
    return std::abs(a - b) < epsilon;
}

// Helper function to compare vectors with epsilon.
bool almostEqual(const Vector2d& a, const Vector2d& b, double epsilon = 1e-6)
{
    return almostEqual(a.x, b.x, epsilon) && almostEqual(a.y, b.y, epsilon);
}

TEST(Vector2dTest, Constructors)
{
    spdlog::info("Starting Vector2dTest::Constructors test");
    // Default constructor.
    Vector2d v1;
    EXPECT_TRUE(almostEqual(v1.x, 0.0));
    EXPECT_TRUE(almostEqual(v1.y, 0.0));

    // Parameterized constructor.
    Vector2d v2(1.0, 2.0);
    EXPECT_TRUE(almostEqual(v2.x, 1.0));
    EXPECT_TRUE(almostEqual(v2.y, 2.0));
}

TEST(Vector2dTest, Operators)
{
    spdlog::info("Starting Vector2dTest::Operators test");
    Vector2d v1(1.0, 2.0);
    Vector2d v2(3.0, 4.0);

    // Addition.
    Vector2d sum = v1 + v2;
    EXPECT_TRUE(almostEqual(sum, Vector2d{ 4.0, 6.0 }));

    // Subtraction.
    Vector2d diff = v2 - v1;
    EXPECT_TRUE(almostEqual(diff, Vector2d{ 2.0, 2.0 }));

    // Scalar multiplication.
    Vector2d scaled = v1 * 2.0;
    EXPECT_TRUE(almostEqual(scaled, Vector2d{ 2.0, 4.0 }));

    // Scalar division.
    Vector2d divided = v2 / 2.0;
    EXPECT_TRUE(almostEqual(divided, Vector2d{ 1.5, 2.0 }));

    // In-place addition.
    v1 += v2;
    EXPECT_TRUE(almostEqual(v1, Vector2d{ 4.0, 6.0 }));

    // In-place subtraction.
    v1 -= v2;
    EXPECT_TRUE(almostEqual(v1, Vector2d{ 1.0, 2.0 }));

    // In-place multiplication.
    v1 *= 2.0;
    EXPECT_TRUE(almostEqual(v1, Vector2d{ 2.0, 4.0 }));

    // In-place division.
    v1 /= 2.0;
    EXPECT_TRUE(almostEqual(v1, Vector2d{ 1.0, 2.0 }));

    // Equality.
    Vector2d expected{ 1.0, 2.0 };
    EXPECT_TRUE(v1 == expected);
    EXPECT_FALSE(v1 == v2);
}

TEST(Vector2dTest, VectorOperations)
{
    spdlog::info("Starting Vector2dTest::VectorOperations test");
    Vector2d v1(3.0, 4.0);
    Vector2d v2(1.0, 2.0);

    // Magnitude.
    EXPECT_TRUE(almostEqual(v1.mag(), 5.0));

    // Dot product.
    EXPECT_TRUE(almostEqual(v1.dot(v2), 11.0));

    // Normalization.
    Vector2d normalized = v1.normalize();
    EXPECT_TRUE(almostEqual(normalized.mag(), 1.0));
    EXPECT_TRUE(almostEqual(normalized, Vector2d{ 0.6, 0.8 }));

    // Add method.
    Vector2d sum = v1.add(v2);
    EXPECT_TRUE(almostEqual(sum, Vector2d{ 4.0, 6.0 }));

    // Subtract method.
    Vector2d diff = v1.subtract(v2);
    EXPECT_TRUE(almostEqual(diff, Vector2d{ 2.0, 2.0 }));

    // Times method.
    Vector2d scaled = v1.times(2.0);
    EXPECT_TRUE(almostEqual(scaled, Vector2d{ 6.0, 8.0 }));
}

TEST(Vector2dTest, EdgeCases)
{
    spdlog::info("Starting Vector2dTest::EdgeCases test");
    Vector2d v(1.0, 2.0);

    // Division by zero.
    EXPECT_THROW(v / 0.0, std::runtime_error);

    // Normalization of zero vector.
    Vector2d zero;
    Vector2d normalized = zero.normalize();
    EXPECT_TRUE(almostEqual(normalized, zero));
}