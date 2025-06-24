#pragma once

#include "lvgl/src/libs/thorvg/rapidjson/document.h"
#include "lvgl/src/libs/thorvg/rapidjson/writer.h"
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
    double magnitude() const { return mag(); }
    double length() const { return mag(); }
    double dot(const Vector2d& other) const;
    Vector2d normalize() const;
    std::string toString() const;

    // Collision physics operations
    Vector2d reflect(const Vector2d& normal) const;
    double angle() const;
    double angleTo(const Vector2d& other) const;
    Vector2d rotateBy(double radians) const;
    Vector2d perpendicular() const;

    // JSON serialization support
    rapidjson::Value toJson(rapidjson::Document::AllocatorType& allocator) const;
    static Vector2d fromJson(const rapidjson::Value& json);

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

    // Unary operators
    Vector2d operator-() const; // Negation operator
    Vector2d operator+() const; // Unary plus (for completeness)
};

// Non-member operator for scalar multiplication from the left side.
inline Vector2d operator*(double scalar, const Vector2d& v)
{
    return v * scalar;
}
