#include "../Vector2d.h"
#include <cassert>
#include <cmath>
#include <iostream>

// Helper function to compare doubles with epsilon.
bool almostEqual(double a, double b, double epsilon = 1e-6) {
    return std::abs(a - b) < epsilon;
}

// Helper function to compare vectors with epsilon.
bool almostEqual(const Vector2d& a, const Vector2d& b, double epsilon = 1e-6) {
    return almostEqual(a.x, b.x, epsilon) && almostEqual(a.y, b.y, epsilon);
}

void test_constructors() {
    // Default constructor.
    Vector2d v1;
    assert(almostEqual(v1.x, 0.0));
    assert(almostEqual(v1.y, 0.0));

    // Parameterized constructor.
    Vector2d v2(1.0, 2.0);
    assert(almostEqual(v2.x, 1.0));
    assert(almostEqual(v2.y, 2.0));
}

void test_operators() {
    Vector2d v1(1.0, 2.0);
    Vector2d v2(3.0, 4.0);

    // Addition.
    Vector2d sum = v1 + v2;
    assert(almostEqual(sum, Vector2d(4.0, 6.0)));

    // Subtraction.
    Vector2d diff = v2 - v1;
    assert(almostEqual(diff, Vector2d(2.0, 2.0)));

    // Scalar multiplication.
    Vector2d scaled = v1 * 2.0;
    assert(almostEqual(scaled, Vector2d(2.0, 4.0)));

    // Scalar division.
    Vector2d divided = v2 / 2.0;
    assert(almostEqual(divided, Vector2d(1.5, 2.0)));

    // In-place addition.
    v1 += v2;
    assert(almostEqual(v1, Vector2d(4.0, 6.0)));

    // In-place subtraction.
    v1 -= v2;
    assert(almostEqual(v1, Vector2d(1.0, 2.0)));

    // In-place multiplication.
    v1 *= 2.0;
    assert(almostEqual(v1, Vector2d(2.0, 4.0)));

    // In-place division.
    v1 /= 2.0;
    assert(almostEqual(v1, Vector2d(1.0, 2.0)));

    // Equality.
    assert(v1 == Vector2d(1.0, 2.0));
    assert(!(v1 == v2));
}

void test_vector_operations() {
    Vector2d v1(3.0, 4.0);
    Vector2d v2(1.0, 2.0);

    // Magnitude.
    assert(almostEqual(v1.mag(), 5.0));

    // Dot product.
    assert(almostEqual(v1.dot(v2), 11.0));

    // Normalization.
    Vector2d normalized = v1.normalize();
    assert(almostEqual(normalized.mag(), 1.0));
    assert(almostEqual(normalized, Vector2d(0.6, 0.8)));

    // Add method.
    Vector2d sum = v1.add(v2);
    assert(almostEqual(sum, Vector2d(4.0, 6.0)));

    // Subtract method.
    Vector2d diff = v1.subtract(v2);
    assert(almostEqual(diff, Vector2d(2.0, 2.0)));

    // Times method.
    Vector2d scaled = v1.times(2.0);
    assert(almostEqual(scaled, Vector2d(6.0, 8.0)));
}

void test_edge_cases() {
    Vector2d v(1.0, 2.0);

    // Division by zero.
    bool caught = false;
    try {
        v / 0.0;
    } catch (const std::runtime_error&) {
        caught = true;
    }
    assert(caught);

    // Normalization of zero vector.
    Vector2d zero;
    Vector2d normalized = zero.normalize();
    assert(almostEqual(normalized, zero));
}

void run_vector2d_tests() {
    std::cout << "Running Vector2d tests...\n";
    
    test_constructors();
    test_operators();
    test_vector_operations();
    test_edge_cases();
    
    std::cout << "All Vector2d tests passed!\n";
}

int main() {
    run_vector2d_tests();
    return 0;
} 