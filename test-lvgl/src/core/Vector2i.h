#pragma once

#include <string>

namespace DirtSim {

class Vector2i {
public:
    int x;
    int y;

    Vector2i();
    Vector2i(int x, int y);

    Vector2i add(const Vector2i& other) const;
    Vector2i subtract(const Vector2i& other) const;
    Vector2i times(int scalar) const;
    double mag() const;
    double length() const { return mag(); }
    int dot(const Vector2i& other) const;
    Vector2i normalize() const;
    std::string toString() const;

    Vector2i operator+(const Vector2i& other) const;
    Vector2i operator-(const Vector2i& other) const;
    Vector2i operator*(int scalar) const;
    bool operator==(const Vector2i& other) const;
    Vector2i& operator+=(const Vector2i& other);
    Vector2i& operator-=(const Vector2i& other);
    Vector2i& operator*=(int scalar);
    Vector2i operator/(int scalar) const;
    Vector2i& operator/=(int scalar);

    // Unary operators
    Vector2i operator-() const; // Negation operator
    Vector2i operator+() const; // Unary plus (for completeness)
};

inline Vector2i operator*(int scalar, const Vector2i& v)
{
    return v * scalar;
}

} // namespace DirtSim