#pragma once

#include <string>

class Vector2d {
public:
    double x;
    double y;

    Vector2d();
    Vector2d(double x, double y);

    Vector2d add(const Vector2d& other) const;
    Vector2d subtract(const Vector2d& other) const;
    Vector2d times(double scalar) const;
    double mag() const;
    double length() const { return mag(); } // Alias for mag()
    double dot(const Vector2d& other) const;
    Vector2d normalize() const;
    std::string toString() const;

    // Operator overloads for more natural syntax.
    Vector2d operator+(const Vector2d& other) const;
    Vector2d operator-(const Vector2d& other) const;
    Vector2d operator*(double scalar) const;
    bool operator==(const Vector2d& other) const;
    Vector2d& operator+=(const Vector2d& other);
    Vector2d& operator-=(const Vector2d& other);
    Vector2d& operator*=(double scalar);
    Vector2d operator/(double scalar) const;
    Vector2d& operator/=(double scalar);
};

// Non-member operator for scalar multiplication from the left side.
inline Vector2d operator*(double scalar, const Vector2d& v)
{
    return v * scalar;
}